use anyhow::Result;
use clap::Parser;

#[derive(Parser)]
struct Args {
    #[arg(long)]
    input: Option<String>,
}

fn main() -> Result<()> {
    let args = Args::parse();
    scheduler_decoder::decode_schedule(args.input.as_deref())
}
