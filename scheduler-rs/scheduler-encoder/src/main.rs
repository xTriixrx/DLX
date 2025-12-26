use anyhow::Result;
use clap::Parser;

#[derive(Parser)]
struct Args {
    #[arg(long)]
    input: String,
}

fn main() -> Result<()> {
    let args = Args::parse();
    scheduler_encoder::encode_schedule(&args.input)
}
