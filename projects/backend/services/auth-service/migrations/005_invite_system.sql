-- Migration: 005_invite_system.sql
-- Таблица инвайт-токенов для закрытой регистрации.

CREATE TABLE invite_tokens (
    id          uuid        PRIMARY KEY DEFAULT gen_random_uuid(),
    token       uuid        NOT NULL UNIQUE DEFAULT gen_random_uuid(),
    created_by  uuid        NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    email_hint  text,
    expires_at  timestamptz NOT NULL,
    used_at     timestamptz,
    used_by     uuid        REFERENCES users(id) ON DELETE SET NULL,
    created_at  timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX invite_tokens_token_idx ON invite_tokens (token);
CREATE INDEX invite_tokens_created_by_idx ON invite_tokens (created_by);
