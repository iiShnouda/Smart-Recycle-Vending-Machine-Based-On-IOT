"""
ReWinGo MongoDB backend — runs on the Raspberry Pi alongside the kiosk app.

Talks to MongoDB Atlas using the official Python driver (pymongo) and
exposes a tiny HTTP API on localhost:5000 that mirrors what the Qt
MongoClient class expects.

Endpoints:
    POST /find         { collection, filter }            → { documents: [...] }
    POST /findOne      { collection, filter }            → { document: {...} }
    POST /insertOne    { collection, document }          → { insertedId: "..." }
    POST /insertMany   { collection, documents: [...] }  → { count: N }
    POST /updateOne    { collection, filter, update }    → { modifiedCount: N }
    POST /deleteOne    { collection, filter }            → { deletedCount: N }

Auth: a static API key in the `x-api-key` header. Set both:
    MONGO_URI         — the connection string from Atlas
    BACKEND_API_KEY   — any long random string you pick

"""

import os
import json
from datetime import datetime
from flask import Flask, request, jsonify
from pymongo import MongoClient
from bson import ObjectId
from dotenv import load_dotenv

load_dotenv()

MONGO_URI = os.getenv("MONGO_URI")
API_KEY   = os.getenv("BACKEND_API_KEY", "")
DB_NAME   = os.getenv("MONGO_DB", "rewingo")

if not MONGO_URI:
    raise RuntimeError("MONGO_URI not set — see .env.example")

client = MongoClient(MONGO_URI)
db = client[DB_NAME]

app = Flask(__name__)


# ── Helpers ────────────────────────────────────────────────────────────────

def auth_ok(req):
    """All endpoints require the API key in the x-api-key header."""
    if not API_KEY:
        return True   # auth disabled in dev
    return req.headers.get("x-api-key") == API_KEY


def to_jsonable(obj):
    """Convert MongoDB BSON types (ObjectId, datetime) to JSON-friendly."""
    if isinstance(obj, ObjectId):
        return str(obj)
    if isinstance(obj, datetime):
        return obj.isoformat()
    if isinstance(obj, dict):
        return {k: to_jsonable(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [to_jsonable(v) for v in obj]
    return obj


@app.before_request
def check_auth():
    if request.method == "OPTIONS":
        return  # let CORS preflight through
    if not auth_ok(request):
        return jsonify({"error": "unauthorised"}), 401


# ── CRUD endpoints ─────────────────────────────────────────────────────────

@app.post("/find")
def find():
    body = request.get_json(force=True)
    coll = db[body["collection"]]
    docs = list(coll.find(body.get("filter", {})).limit(1000))
    return jsonify({"documents": to_jsonable(docs)})


@app.post("/findOne")
def find_one():
    body = request.get_json(force=True)
    coll = db[body["collection"]]
    doc = coll.find_one(body.get("filter", {}))
    return jsonify({"document": to_jsonable(doc) if doc else None})


@app.post("/insertOne")
def insert_one():
    body = request.get_json(force=True)
    coll = db[body["collection"]]
    result = coll.insert_one(body["document"])
    return jsonify({"insertedId": str(result.inserted_id)})


@app.post("/insertMany")
def insert_many():
    body = request.get_json(force=True)
    coll = db[body["collection"]]
    result = coll.insert_many(body["documents"])
    return jsonify({"count": len(result.inserted_ids)})


@app.post("/updateOne")
def update_one():
    body = request.get_json(force=True)
    coll = db[body["collection"]]
    upsert = body.get("upsert", False)
    result = coll.update_one(body["filter"], body["update"], upsert=upsert)
    return jsonify({"modifiedCount": result.modified_count,
                    "upsertedId":    str(result.upserted_id) if result.upserted_id else None})


@app.post("/deleteOne")
def delete_one():
    body = request.get_json(force=True)
    coll = db[body["collection"]]
    result = coll.delete_one(body["filter"])
    return jsonify({"deletedCount": result.deleted_count})


@app.get("/health")
def health():
    try:
        client.admin.command("ping")
        return jsonify({"status": "ok", "db": DB_NAME})
    except Exception as e:
        return jsonify({"status": "error", "msg": str(e)}), 500


if __name__ == "__main__":
    # Bind to localhost only — the kiosk app is the only client.
    app.run(host="127.0.0.1", port=5000, debug=False)
