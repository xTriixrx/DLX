#!/usr/bin/env python3
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path
import atexit
import os

HOST = "127.0.0.1"
REQ_PORT = 5555
SOL_PORT = 5556
ENCODER_BIN = Path("./build/sudoku_encoder")
DECODER_BIN = Path("./build/sudoku_decoder")
DLX_IMAGE = "dlx:latest"
DLX_CONTAINER = "dlx-solver"
DLX_PLATFORM = os.environ.get("DLX_PLATFORM")

DLXS_MAGIC_BYTES = 4
DLXS_VERSION_BYTES = 2
DLXS_FLAGS_BYTES = 2
DLXS_COLUMN_COUNT_BYTES = 4

DLXS_HEADER_SIZE = (
    DLXS_MAGIC_BYTES
    + DLXS_VERSION_BYTES
    + DLXS_FLAGS_BYTES
    + DLXS_COLUMN_COUNT_BYTES
)

DLXS_SOLUTION_ID_BYTES = 4
DLXS_ENTRY_COUNT_BYTES = 2
DLXS_ROW_HEADER_SIZE = DLXS_SOLUTION_ID_BYTES + DLXS_ENTRY_COUNT_BYTES
DLXS_ROW_VALUE_BYTES = 4


def start_server():
    platform_args = ["--platform", DLX_PLATFORM] if DLX_PLATFORM else []
    return subprocess.Popen(
        [
            "docker",
            "run",
            *platform_args,
            "--rm",
            "--name",
            DLX_CONTAINER,
            "-p",
            f"{REQ_PORT}:7000",
            "-p",
            f"{SOL_PORT}:7001",
            "-e",
            "DLX_PROBLEM_PORT=7000",
            "-e",
            "DLX_SOLUTION_PORT=7001",
            DLX_IMAGE,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

def wait_for_port(port: int, proc: subprocess.Popen, timeout: float = 10.0) -> socket.socket:
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        if proc.poll() is not None:
            stderr = proc.stderr.read().decode() if proc.stderr else ""
            stdout = proc.stdout.read().decode() if proc.stdout else ""
            raise RuntimeError(
                "Docker server exited early."
                + (f"\nstdout: {stdout.strip()}" if stdout else "")
                + (f"\nstderr: {stderr.strip()}" if stderr else "")
            )
        try:
            return socket.create_connection((HOST, port), timeout=1)
        except OSError as exc:
            last_error = exc
            time.sleep(0.2)
    raise RuntimeError(f"Timed out waiting for port {port}: {last_error}")

def stop_server(proc):
    subprocess.run(["docker", "stop", "-t", "2", DLX_CONTAINER], check=False)
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

def send_problem(puzzle_file: Path):
    encoder = subprocess.Popen([str(ENCODER_BIN), str(puzzle_file)], stdout=subprocess.PIPE)
    assert encoder.stdout is not None
    req_sock = socket.create_connection((HOST, REQ_PORT))
    try:
        with encoder.stdout as src:
            while True:
                chunk = src.read(65536)
                if not chunk:
                    break
                req_sock.sendall(chunk)
        encoder.wait()
        if encoder.returncode != 0:
            raise RuntimeError("Sudoku encoder failed")
    finally:
        req_sock.shutdown(socket.SHUT_WR)
        req_sock.close()

def read_dlxs_frame(solution_sock: socket.socket) -> bytes:
    def read_exact(size: int) -> bytes:
        chunks = bytearray()
        while len(chunks) < size:
            data = solution_sock.recv(size - len(chunks))
            if not data:
                break
            chunks.extend(data)
        if len(chunks) != size:
            raise RuntimeError(f"Incomplete DLXS stream (got {len(chunks)} of {size} bytes)")
        return bytes(chunks)

    frame = bytearray()
    header = read_exact(DLXS_HEADER_SIZE)
    if header[:DLXS_MAGIC_BYTES] != b"DLXS":
        raise RuntimeError("Invalid DLXS magic")
    frame.extend(header)

    while True:
        row_header = read_exact(DLXS_ROW_HEADER_SIZE)
        frame.extend(row_header)
        solution_id = struct.unpack("!I", row_header[:4])[0]
        entry_count = struct.unpack("!H", row_header[4:])[0]
        payload = read_exact(entry_count * DLXS_ROW_VALUE_BYTES)
        frame.extend(payload)
        if solution_id == 0 and entry_count == 0:
            break

    return bytes(frame)

def solve_puzzle(puzzle_lines, solution_sock: socket.socket):
    if len(puzzle_lines) != 9:
        raise ValueError("Puzzle must contain exactly 9 lines")
    with tempfile.NamedTemporaryFile("w", delete=False) as tmp:
        tmp.write("\n".join(puzzle_lines) + "\n")
        tmp_path = Path(tmp.name)
    try:
        send_problem(tmp_path)
        frame = read_dlxs_frame(solution_sock)
        decoder = subprocess.run(
            [str(DECODER_BIN), str(tmp_path)],
            input=frame,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if decoder.returncode != 0:
            sys.stderr.write(decoder.stderr.decode())
        sys.stdout.write(decoder.stdout.decode())
        sys.stdout.flush()
    finally:
        tmp_path.unlink(missing_ok=True)

def main():
    server_proc = start_server()

    def handle_signal(*_):
        stop_server(server_proc)
        sys.exit(0)

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGHUP, handle_signal)
    signal.signal(signal.SIGQUIT, handle_signal)
    atexit.register(stop_server, server_proc)

    solution_sock = wait_for_port(SOL_PORT, server_proc)

    print("Enter Sudoku puzzles (9 lines each). Separate puzzles with a blank line.")
    buffer = []
    for raw_line in sys.stdin:
        line = raw_line.strip()
        if not line:
            continue
        if len(line) != 9:
            print("Each line must contain 9 digits; skipping.")
            continue
        buffer.append(line)
        if len(buffer) == 9:
            try:
                solve_puzzle(buffer, solution_sock)
            except Exception as exc:
                print(f"Error solving puzzle: {exc}", file=sys.stderr)
                break
            buffer.clear()
            print("Ready for next puzzle...")

    if buffer:
        print("Incomplete puzzle ignored", file=sys.stderr)

    solution_sock.close()
    stop_server(server_proc)

if __name__ == "__main__":
    main()
