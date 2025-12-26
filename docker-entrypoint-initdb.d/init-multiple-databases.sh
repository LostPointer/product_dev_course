#!/bin/bash
set -e

# Create auth_db if it doesn't exist
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<-EOSQL
    DO \$\$
    BEGIN
        IF NOT EXISTS (SELECT FROM pg_database WHERE datname = 'auth_db') THEN
            CREATE DATABASE auth_db;
        END IF;
    END
    \$\$;
EOSQL

echo "✅ Multiple databases initialized"

