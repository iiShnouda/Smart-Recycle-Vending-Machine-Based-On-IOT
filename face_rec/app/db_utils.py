import sqlite3
import numpy as np
from pathlib import Path

DB_PATH = Path("db") / "faces.db"


def _connect():
    return sqlite3.connect(DB_PATH)


def init_db():
    DB_PATH.parent.mkdir(exist_ok=True)
    con = _connect()
    cur = con.cursor()
    cur.execute("""
    CREATE TABLE IF NOT EXISTS users (
        user_id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        embedding BLOB NOT NULL,
        created_at TEXT DEFAULT CURRENT_TIMESTAMP
    )
    """)
    # Migration: add the phone column to pre-existing databases. SQLite has no
    # "ADD COLUMN IF NOT EXISTS", so just try it and ignore the duplicate error.
    try:
        cur.execute("ALTER TABLE users ADD COLUMN phone TEXT")
    except sqlite3.OperationalError:
        pass
    con.commit()
    con.close()


def insert_user(name: str, embedding: np.ndarray, phone: str = None) -> int:
    con = _connect()
    cur = con.cursor()
    emb_bytes = embedding.astype(np.float32).tobytes()
    cur.execute("INSERT INTO users(name, embedding, phone) VALUES (?, ?, ?)",
                (name, emb_bytes, phone))
    con.commit()
    user_id = cur.lastrowid
    con.close()
    return user_id


def load_users():
    con = _connect()
    cur = con.cursor()
    cur.execute("SELECT user_id, name, embedding FROM users")
    rows = cur.fetchall()
    con.close()

    users = []
    for user_id, name, emb_bytes in rows:
        emb = np.frombuffer(emb_bytes, dtype=np.float32)
        users.append((user_id, name, emb))
    return users
