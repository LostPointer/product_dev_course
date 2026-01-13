BEGIN;

-- Audit log for run actions (status changes, bulk tag updates, etc.)
CREATE TABLE IF NOT EXISTS run_events (
    id bigserial PRIMARY KEY,
    run_id uuid NOT NULL,
    event_type text NOT NULL,
    actor_id uuid NOT NULL,
    actor_role text NOT NULL,
    payload jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (run_id) REFERENCES runs (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS run_events_run_idx ON run_events (run_id, created_at DESC);

COMMIT;

