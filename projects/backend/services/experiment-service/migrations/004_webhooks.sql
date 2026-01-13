BEGIN;

-- Webhook subscriptions scoped to a project
CREATE TABLE IF NOT EXISTS webhook_subscriptions (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    project_id uuid NOT NULL,
    target_url text NOT NULL,
    secret text,
    event_types text[] NOT NULL DEFAULT '{}'::text[],
    is_active boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS webhook_subscriptions_project_idx
    ON webhook_subscriptions (project_id, created_at DESC);

CREATE INDEX IF NOT EXISTS webhook_subscriptions_event_types_gin_idx
    ON webhook_subscriptions USING gin (event_types);

CREATE TRIGGER webhook_subscriptions_set_updated_at
    BEFORE UPDATE ON webhook_subscriptions
    FOR EACH ROW
    EXECUTE FUNCTION set_updated_at();


-- Outbox for webhook deliveries (best-effort, at-least-once)
CREATE TABLE IF NOT EXISTS webhook_deliveries (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    subscription_id uuid NOT NULL,
    project_id uuid NOT NULL,
    event_type text NOT NULL,
    target_url text NOT NULL,
    secret text,
    request_body jsonb NOT NULL,
    status text NOT NULL DEFAULT 'pending', -- pending|succeeded|failed
    attempt_count integer NOT NULL DEFAULT 0,
    last_error text,
    next_attempt_at timestamptz NOT NULL DEFAULT now(),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (subscription_id) REFERENCES webhook_subscriptions (id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS webhook_deliveries_pending_idx
    ON webhook_deliveries (status, next_attempt_at);

CREATE INDEX IF NOT EXISTS webhook_deliveries_project_idx
    ON webhook_deliveries (project_id, created_at DESC);

CREATE TRIGGER webhook_deliveries_set_updated_at
    BEFORE UPDATE ON webhook_deliveries
    FOR EACH ROW
    EXECUTE FUNCTION set_updated_at();

COMMIT;

