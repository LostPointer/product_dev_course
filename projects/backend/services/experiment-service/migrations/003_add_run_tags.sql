-- 003_add_run_tags.sql
-- Add tags support for runs to enable bulk tagging operations.

BEGIN;

ALTER TABLE runs
    ADD COLUMN IF NOT EXISTS tags text[] NOT NULL DEFAULT '{}'::text[];

-- Optional: speed up tag filtering in future
CREATE INDEX IF NOT EXISTS runs_tags_gin_idx ON runs USING gin (tags);

COMMIT;

