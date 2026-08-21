$ErrorActionPreference = "Stop"
docker compose -f docker/docker-compose.yml up --build -d
try {
  Start-Sleep -Seconds 5
  docker compose -f docker/docker-compose.yml exec -T node-a /bin/sh -lc "printf 'node failure simulation' > /tmp/failure-object.txt"
  docker compose -f docker/docker-compose.yml stop node-c
  docker compose -f docker/docker-compose.yml exec -T node-a /app/dstore-cli --host node-a --port 8080 --token admin-token upload failure/object /tmp/failure-object.txt
  docker compose -f docker/docker-compose.yml start node-c
  Start-Sleep -Seconds 5
  docker compose -f docker/docker-compose.yml exec -T node-a /bin/sh -lc "printf '' | /app/dstore-cli --host node-a --port 8080 --token admin-token metadata failure/object"
}
finally {
  docker compose -f docker/docker-compose.yml down
}
