$ErrorActionPreference = "Stop"
docker compose -f docker/docker-compose.yml up --build -d
try {
  Start-Sleep -Seconds 5
  docker compose -f docker/docker-compose.yml exec -T node-a /bin/sh -lc "printf 'hello distributed storage' > /tmp/smoke-object.txt"
  docker compose -f docker/docker-compose.yml exec -T node-a /app/dstore-cli --host node-a --port 8080 --token admin-token upload smoke/object /tmp/smoke-object.txt
  docker compose -f docker/docker-compose.yml stop node-b
  docker compose -f docker/docker-compose.yml exec -T node-a /app/dstore-cli --host node-a --port 8080 --token admin-token download smoke/object /tmp/restored.txt
  docker compose -f docker/docker-compose.yml exec -T node-a /bin/sh -lc "cmp /tmp/smoke-object.txt /tmp/restored.txt"
}
finally {
  docker compose -f docker/docker-compose.yml down
}
