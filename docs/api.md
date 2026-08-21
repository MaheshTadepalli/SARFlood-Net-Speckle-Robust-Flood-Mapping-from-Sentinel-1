# REST API

All object APIs require `Authorization: Bearer <token>`.

## Upload

`PUT /v1/objects/{bucket}/{key}`

Request body is the raw object bytes.

Response:

```json
{"bucket":"photos","key":"cat.jpg","checksum":"...","chunks":1}
```

## Download

`GET /v1/objects/{bucket}/{key}`

Response body is the raw object bytes.

## Delete

`DELETE /v1/objects/{bucket}/{key}`

Returns `204 No Content` when deleted.

## Metadata

`GET /v1/metadata/{bucket}/{key}`

Returns size, checksum, and chunk count.

## Heartbeat

`POST /v1/heartbeat`

Admin-only endpoint used by storage nodes to refresh liveness.

## Metrics

`GET /metrics`

Prometheus text format counters.

## Internal Chunk RPC

Admin-only APIs used by peer nodes:

- `PUT /v1/chunks/{checksum}`
- `GET /v1/chunks/{checksum}`
- `DELETE /v1/chunks/{checksum}`

These endpoints are intentionally not part of the public object API.

## Admin Operations

Admin-only APIs:

- `POST /v1/admin/repair/{bucket}`: rewrites known chunks through the distributed chunk service to restore missing replicas.
- `POST /v1/admin/rebalance/{bucket}`: recomputes placement after membership changes and rewrites chunks to desired targets.
- `POST /v1/admin/handoff/replay`: replays hinted handoff entries for replicas that were unavailable during writes.
