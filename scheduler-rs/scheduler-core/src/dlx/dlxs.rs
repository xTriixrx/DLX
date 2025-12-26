use crate::dlx::dlxb::{DlxbRowChunk, DLX_BINARY_VERSION};
use anyhow::{anyhow, bail, ensure, Context, Result};
use std::convert::TryFrom;
use std::io::{self, Read, Write};

pub const DLXS_MAGIC: u32 = 0x444C_5853; // 'DLXS'
pub const DLXS_SENTINEL_SOLUTION_ID: u32 = 0;
pub const DLXS_SENTINEL_ENTRY_COUNT: u16 = 0;
const HEADER_SIZE: usize = 4 + 2 + 2 + 4;
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

/// DLXS header emitted before the solver streams solution rows.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DlxsHeader {
    /// ASCII ``"DLXS"`` sentinel written before every solution frame.
    pub magic: u32,
    /// Protocol version matching the originating DLXB frame.
    pub version: u16,
    /// Reserved flags for future transport metadata.
    pub flags: u16,
    /// Column count required to interpret row references.
    pub column_count: u32,
}

impl DlxsHeader {
    /// Create a DLXS header for the specified column count.
    pub fn new(column_count: u32) -> Self {
        Self {
            magic: DLXS_MAGIC,
            version: DLX_BINARY_VERSION,
            flags: 0,
            column_count,
        }
    }

    /// Ensure the header matches the expected protocol settings.
    pub fn validate(&self) -> Result<()> {
        ensure!(
            self.magic == DLXS_MAGIC,
            "invalid DLXS magic {:08X}",
            self.magic
        );
        ensure!(
            self.version == DLX_BINARY_VERSION,
            "unsupported DLXS version {}",
            self.version
        );
        Ok(())
    }

    /// Serialize the header into network byte order.
    pub fn write_to<W: Write>(&self, writer: &mut W) -> Result<()> {
        self.validate()?;
        write_u32(writer, self.magic)?;
        write_u16(writer, self.version)?;
        write_u16(writer, self.flags)?;
        write_u32(writer, self.column_count)?;
        Ok(())
    }

    /// Parse a header from the reader, returning `Ok(None)` if EOF is reached.
    pub fn read_from<R: Read>(reader: &mut R) -> Result<Option<Self>> {
        let mut buf = [0u8; HEADER_SIZE];
        let Some(()) = read_exact_or_none(reader, &mut buf)? else {
            return Ok(None);
        };

        let magic = u32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]);
        let version = u16::from_be_bytes([buf[4], buf[5]]);
        let flags = u16::from_be_bytes([buf[6], buf[7]]);
        let column_count = u32::from_be_bytes([buf[8], buf[9], buf[10], buf[11]]);

        let header = Self {
            magic,
            version,
            flags,
            column_count,
        };
        header.validate()?;
        Ok(Some(header))
    }

    /// Number of bytes written when encoding the header.
    pub fn encoded_len(&self) -> usize {
        HEADER_SIZE
    }
}

/// Individual solution row within a DLXS frame.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DlxsSolutionRow {
    /// Sequential identifier assigned as solutions stream out.
    pub solution_id: u32,
    /// Row identifiers selected from the DLXB stream.
    pub indices: Vec<u32>,
}

impl DlxsSolutionRow {
    /// Create a new solution row with the specified identifier and row indices.
    pub fn new(solution_id: u32, indices: Vec<u32>) -> Self {
        Self {
            solution_id,
            indices,
        }
    }

    /// Construct the sentinel row used as an end-of-problem marker.
    pub fn sentinel() -> Self {
        Self {
            solution_id: DLXS_SENTINEL_SOLUTION_ID,
            indices: Vec::new(),
        }
    }

    /// Returns true if this row is the sentinel marker.
    pub fn is_sentinel(&self) -> bool {
        self.solution_id == DLXS_SENTINEL_SOLUTION_ID && self.indices.is_empty()
    }

    /// Serialize the solution row to the provided writer.
    pub fn write_to<W: Write>(&self, writer: &mut W) -> Result<()> {
        let entry_count = u16::try_from(self.indices.len())
            .context("DLXS solution rows cannot exceed 65,535 entries")?;
        write_u32(writer, self.solution_id)?;
        write_u16(writer, entry_count)?;
        for &index in &self.indices {
            write_u32(writer, index)?;
        }
        Ok(())
    }

    /// Deserialize a solution row from the reader.
    pub fn read_from<R: Read>(reader: &mut R) -> Result<Self> {
        let solution_id = read_u32(reader)?;
        let entry_count = read_u16(reader)? as usize;
        let mut indices = Vec::with_capacity(entry_count);
        for _ in 0..entry_count {
            indices.push(read_u32(reader)?);
        }
        Ok(Self {
            solution_id,
            indices,
        })
    }
}

/// Represents one complete DLXS minor frame (header + solutions).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DlxsPacket {
    /// Header describing the upcoming solution rows.
    pub header: DlxsHeader,
    /// Collection of solution rows (excluding the sentinel).
    pub rows: Vec<DlxsSolutionRow>,
}

impl DlxsPacket {
    /// Build a packet from the supplied header metadata and solution rows.
    pub fn new(column_count: u32, rows: Vec<DlxsSolutionRow>) -> Result<Self> {
        for row in &rows {
            ensure!(
                !row.is_sentinel(),
                "sentinel rows are appended automatically; do not include them manually"
            );
        }
        Ok(Self {
            header: DlxsHeader::new(column_count),
            rows,
        })
    }

    /// Serialize the packet (header + rows + sentinel) to the writer.
    pub fn write_to<W: Write>(&self, writer: &mut W) -> Result<()> {
        self.header.write_to(writer)?;
        for row in &self.rows {
            row.write_to(writer)?;
        }
        DlxsSolutionRow::sentinel().write_to(writer)?;
        Ok(())
    }

    /// Attempt to read a packet from the reader, yielding `Ok(None)` on EOF.
    pub fn read_from<R: Read>(reader: &mut R) -> Result<Option<Self>> {
        let Some(header) = DlxsHeader::read_from(reader)? else {
            return Ok(None);
        };

        let mut rows = Vec::new();
        loop {
            let row = DlxsSolutionRow::read_from(reader)?;
            if row.is_sentinel() {
                break;
            }
            rows.push(row);
        }
        Ok(Some(Self { header, rows }))
    }

    /// Total encoded length of the packet, including the sentinel row.
    pub fn encoded_len(&self) -> usize {
        self.header.encoded_len()
            + self
                .rows
                .iter()
                .map(|row| ROW_HEADER_SIZE + row.indices.len() * 4)
                .sum::<usize>()
            + ROW_HEADER_SIZE
    }

    /// Expand each solution into a list of column indices using the original DLXB rows.
    pub fn apply_to_rows(&self, cover_rows: &[DlxbRowChunk]) -> Vec<Vec<u32>> {
        let mut solutions = Vec::with_capacity(self.rows.len());
        for row in &self.rows {
            let mut expanded = Vec::with_capacity(row.indices.len());
            for &row_id in &row.indices {
                if let Some(chunk) = cover_rows.iter().find(|chunk| chunk.row_id == row_id) {
                    expanded.extend(&chunk.columns);
                }
            }
            solutions.push(expanded);
        }
        solutions
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[test]
    fn dlxs_sentinel_round_trip() -> Result<()> {
        let rows = vec![
            DlxsSolutionRow::new(1, vec![10, 11, 12]),
            DlxsSolutionRow::new(2, vec![20, 21, 22]),
        ];
        let packet = DlxsPacket::new(40, rows.clone())?;

        let mut bytes = Vec::new();
        packet.write_to(&mut bytes)?;

        let mut cursor = Cursor::new(bytes);
        let decoded = DlxsPacket::read_from(&mut cursor)?.expect("packet should exist");
        assert_eq!(decoded.header.column_count, 40);
        assert_eq!(decoded.rows, rows);
        Ok(())
    }

    #[test]
    fn dlxs_header_optional() -> Result<()> {
        let mut cursor = Cursor::new(Vec::new());
        let header = DlxsHeader::read_from(&mut cursor)?;
        assert!(header.is_none());
        Ok(())
    }
}
