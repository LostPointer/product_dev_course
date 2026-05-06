import type { Config } from 'jest'

const config: Config = {
    preset: 'ts-jest',
    testEnvironment: 'node',
    testMatch: ['<rootDir>/test/**/*.test.ts'],
    clearMocks: true,
    restoreMocks: true,
    verbose: false,
    coverageDirectory: 'coverage',
    coverageReporters: ['text', 'html', 'lcov'],
    collectCoverageFrom: [
        'src/**/*.ts',
        '!src/**/*.d.ts',
        '!src/**/__mocks__/**',
    ],
}

export default config

