//! Networking helpers for talking to the DLX TCP server.
//!
//! The module is intentionally lightweight right now; higher-level clients
//! will evolve alongside the encoder/decoder crates.

#[derive(Debug, Clone)]
pub struct TcpEndpoint {
    pub host: String,
    pub port: u16,
}

impl TcpEndpoint {
    pub fn new(host: impl Into<String>, port: u16) -> Self {
        Self {
            host: host.into(),
            port,
        }
    }

    pub fn to_string(&self) -> String {
        format!("{}:{}", self.host, self.port)
    }
}
