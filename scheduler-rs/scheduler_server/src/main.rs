use anyhow::Result;
use scheduler_server::router;
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<()> {
    let app = router();
    let listener = TcpListener::bind(("0.0.0.0", 8080)).await?;
    println!("scheduler server listening on 8080");
    axum::serve(listener, app).await?;
    Ok(())
}
