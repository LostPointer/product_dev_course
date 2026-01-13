-- 002_webhooks.sql
-- Webhooks schema (subscriptions + deliveries outbox) used by experiment-service.

BEGIN;

-- Subscriptions: which events to deliver and where
CREATE TABLE webhook_subscriptions (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    project_id uuid NOT NULL,
    target_url text NOT NULL,
    event_types text[] NOT NULL DEFAULT '{}'::text[],
    secret text,
    is_active boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX webhook_subscriptions_project_active_idx
    ON webhook_subscriptions (project_id, is_active);

CREATE INDEX webhook_subscriptions_event_types_gin_idx
    ON webhook_subscriptions USING gin (event_types);

CREATE TRIGGER webhook_subscriptions_set_updated_at
    BEFORE UPDATE ON webhook_subscriptions
    FOR EACH ROW
    EXECUTE FUNCTION set_updated_at();

-- Deliveries outbox: pending deliveries to be processed by dispatcher
CREATE TABLE webhook_deliveries (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    subscription_id uuid NOT NULL,
    project_id uuid NOT NULL,
    event_type text NOT NULL,
    target_url text NOT NULL,
    secret text,
    request_body jsonb NOT NULL DEFAULT '{}'::jsonb,
    dedup_key text UNIQUE,
    status text NOT NULL DEFAULT 'pending', -- pending|in_progress|succeeded|dead_lettered (plus legacy failed)
    attempt_count integer NOT NULL DEFAULT 0,
    last_error text,
    locked_at timestamptz,
    next_attempt_at timestamptz NOT NULL DEFAULT now(),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (subscription_id) REFERENCES webhook_subscriptions (id) ON DELETE CASCADE
);

CREATE INDEX webhook_deliveries_project_status_idx
    ON webhook_deliveries (project_id, status, created_at DESC);

CREATE INDEX webhook_deliveries_due_pending_idx
    ON webhook_deliveries (status, next_attempt_at, created_at);

CREATE TRIGGER webhook_deliveries_set_updated_at
    BEFORE UPDATE ON webhook_deliveries
    FOR EACH ROW
    EXECUTE FUNCTION set_updated_at();

COMMIT;

