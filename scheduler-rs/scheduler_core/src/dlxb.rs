use anyhow::{anyhow, bail, ensure, Context, Result};
use std::convert::TryFrom;
use std::io::{self, Read, Write};

pub const DLXB_MAGIC: u32 = 0x444C_5842; // 'DLXB'
pub const DLX_BINARY_VERSION: u16 = 1;
const HEADER_SIZE: usize = 4 + 2 + 2 + 4 + 4;
const ROW_HEADER_SIZE: usize = 4 + 2;

fn read_exact_or_none<R: Read>(reader: &mut R, buf: &mut [u8]) -> Result<Option<()>> {
    let mut offset = 0;
    while offset < buf.len() {
        match reader.read(&mut buf[offset..]) {
            Ok(0) => {
                if offset == 0 {
                    return Ok(None);
                }
                bail!(
                    "unexpected EOF while reading {} bytes (read {})",
                    buf.len(),
                    offset
                );
            }
            Ok(bytes_read) => offset += bytes_read,
            Err(err) if err.kind() == io::ErrorKind::Interrupted => continue,
            Err(err) => return Err(err.into()),
        }
    }
    Ok(Some(()))
}

fn read_exact_required<R: Read>(reader: &mut R, buf: &mut [u8]) -> Result<()> {
    read_exact_or_none(reader, buf)?.ok_or_else(|| anyhow!("unexpected EOF"))?;
    Ok(())
}

fn write_u32<W: Write>(writer: &mut W, value: u32) -> Result<()> {
    writer.write_all(&value.to_be_bytes()).map_err(|e| e.into())
}

fn write_u16<W: Write>(writer: &mut W, value: u16) -> Result<()> {
    writer.write_all(&value.to_be_bytes()).map_err(|e| e.into())
}

fn read_u32<R: Read>(reader: &mut R) -> Result<u32> {
    let mut buf = [0u8; 4];
    read_exact_required(reader, &mut buf)?;
    Ok(u32::from_be_bytes(buf))
}

fn read_u16<R: Read>(reader: &mut R) -> Result<u16> {
    let mut buf = [0u8; 2];
    read_exact_required(reader, &mut buf)?;
    Ok(u16::from_be_bytes(buf))
}

/// DLXB header that prefixes every problem packet.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DlxbHeader {
    /// ASCII ``"DLXB"`` sentinel stored in network byte order.
    pub magic: u32,
    /// Protocol version understood by both encoder and solver.
    pub version: u16,
    /// Reserved for future transport metadata (always zero today).
    pub flags: u16,
    /// Total number of constraint columns in the exact-cover matrix.
    pub column_count: u32,
    /// Number of row chunks serialized in the packet.
    pub row_count: u32,
}

impl DlxbHeader {
    /// Construct a header with the given column and row counts.
    ///
    /// The `version` and `flags` fields are initialized to protocol defaults.
    pub fn new(column_count: u32, row_count: u32) -> Self {
        Self {
            magic: DLXB_MAGIC,
            version: DLX_BINARY_VERSION,
            flags: 0,
            column_count,
            row_count,
        }
    }

    /// Ensure the header contains supported settings.
    ///
    /// This verifies the magic number and binary version match the solver expectations.
    pub fn validate(&self) -> Result<()> {
        ensure!(
            self.magic == DLXB_MAGIC,
            "invalid DLXB magic {:08X}",
            self.magic
        );
        ensure!(
            self.version == DLX_BINARY_VERSION,
            "unsupported DLXB version {}",
            self.version
        );
        Ok(())
    }

    /// Serialize the header in network byte order to the provided writer.
    pub fn write_to<W: Write>(&self, writer: &mut W) -> Result<()> {
        self.validate()?;
        write_u32(writer, self.magic)?;
        write_u16(writer, self.version)?;
        write_u16(writer, self.flags)?;
        write_u32(writer, self.column_count)?;
        write_u32(writer, self.row_count)?;
        Ok(())
    }

    /// Parse a header from the supplied reader.
    ///
    /// Returns an error if the stream is truncated or contains unsupported metadata.
    pub fn read_from<R: Read>(reader: &mut R) -> Result<Self> {
        let magic = read_u32(reader)?;
        let version = read_u16(reader)?;
        let flags = read_u16(reader)?;
        let column_count = read_u32(reader)?;
        let row_count = read_u32(reader)?;
        let header = Self {
            magic,
            version,
            flags,
            column_count,
            row_count,
        };
        header.validate()?;
        Ok(header)
    }

    /// The number of bytes produced when serializing the header.
    pub fn encoded_len(&self) -> usize {
        HEADER_SIZE
    }
}

/// Serialized DLXB row chunk that contains the indices of populated columns.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DlxbRowChunk {
    /// Unique identifier for the row inside the DLXB stream.
    pub row_id: u32,
    /// Column indices set to ``1`` for this row.
    pub columns: Vec<u32>,
}

impl DlxbRowChunk {
    /// Create a row chunk with the supplied row identifier and column indices.
    pub fn new(row_id: u32, columns: Vec<u32>) -> Self {
        Self { row_id, columns }
    }

    /// Total byte length of the row when serialized (header + column values).
    pub fn encoded_len(&self) -> usize {
        ROW_HEADER_SIZE + self.columns.len() * 4
    }

    /// Write the chunk in DLXB format to the provided writer.
    pub fn write_to<W: Write>(&self, writer: &mut W) -> Result<()> {
        let column_count =
            u16::try_from(self.columns.len()).context("DLXB rows cannot exceed 65,535 entries")?;
        write_u32(writer, self.row_id)?;
        write_u16(writer, column_count)?;
        for &column in &self.columns {
            write_u32(writer, column)?;
        }
        Ok(())
    }

    /// Read a chunk from the given reader.
    ///
    /// Returns an error if the stream terminates early or the chunk is malformed.
    pub fn read_from<R: Read>(reader: &mut R) -> Result<Self> {
        let row_id = read_u32(reader)?;
        let entry_count = read_u16(reader)? as usize;
        let mut columns = Vec::with_capacity(entry_count);
        for _ in 0..entry_count {
            columns.push(read_u32(reader)?);
        }
        Ok(Self { row_id, columns })
    }
}

/// High-level representation of an entire DLXB packet (header + rows).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DlxbPacket {
    /// Header metadata that precedes every DLXB minor frame.
    pub header: DlxbHeader,
    /// Serialized rows that make up the cover matrix.
    pub rows: Vec<DlxbRowChunk>,
}

impl DlxbPacket {
    /// Create an in-memory packet containing a header plus all row chunks.
    pub fn new(column_count: u32, rows: Vec<DlxbRowChunk>) -> Result<Self> {
        let row_count = u32::try_from(rows.len()).context("row count exceeds u32")?;
        Ok(Self {
            header: DlxbHeader::new(column_count, row_count),
            rows,
        })
    }

    /// Serialize the packet (header followed by rows) to the target writer.
    pub fn write_to<W: Write>(&self, writer: &mut W) -> Result<()> {
        self.header.write_to(writer)?;
        for row in &self.rows {
            row.write_to(writer)?;
        }
        Ok(())
    }

    /// Deserialize a DLXB packet from the given reader.
    pub fn read_from<R: Read>(reader: &mut R) -> Result<Self> {
        let header = DlxbHeader::read_from(reader)?;
        let row_count =
            usize::try_from(header.row_count).context("row_count exceeds usize capacity")?;
        let mut rows = Vec::with_capacity(row_count);
        for _ in 0..row_count {
            rows.push(DlxbRowChunk::read_from(reader)?);
        }
        Ok(Self { header, rows })
    }

    /// Total number of bytes required to encode the packet.
    pub fn encoded_len(&self) -> usize {
        self.header.encoded_len()
            + self
                .rows
                .iter()
                .map(DlxbRowChunk::encoded_len)
                .sum::<usize>()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn round_trip_packet() -> Result<()> {
        let rows = vec![
            DlxbRowChunk::new(1, vec![0, 5, 8, 12]),
            DlxbRowChunk::new(2, vec![1, 3, 10, 11]),
        ];
        let packet = DlxbPacket::new(20, rows.clone())?;

        let mut bytes = Vec::new();
        packet.write_to(&mut bytes)?;
        assert_eq!(bytes.len(), packet.encoded_len());

        let mut cursor = Cursor::new(bytes);
        let decoded = DlxbPacket::read_from(&mut cursor)?;
        assert_eq!(decoded.header.column_count, 20);
        assert_eq!(decoded.header.row_count, 2);
        assert_eq!(decoded.rows, rows);
        Ok(())
    }
}
