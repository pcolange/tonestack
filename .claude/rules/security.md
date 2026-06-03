# Security Guidelines

To prevent data leaks, credential exposure, and environment fragmentation, all contributors and AI agents must adhere to these strict security requirements.

## Privacy & Portability

- **No Absolute Paths:** Never use absolute paths (e.g., `/home/user/...`, `C:\Users\...`). Always use **relative paths** to ensure code is portable and does not leak local system information.
- **No Hostnames:** Avoid hardcoding internal or local hostnames. Use environment variables or configuration files to abstract infrastructure details.
- **Environment Agnostic:** All configurations must be valid across different developer machines and CI/CD environments.

## Secrets & Credentials

- **No Hardcoded Secrets:** Never commit API keys, tokens, passwords, or any form of credential to the repository.
- **Secret Management:** Use environment variables, secret managers (e.g., GCP Secret Manager, Vault), or `.env` files (which must be ignored by Git).
- **Template Sanitation:** When creating example files or templates, always use placeholders (e.g., `YOUR_API_KEY_HERE`) rather than real or decommissioned data.

## Infrastructure

- **Internal Endpoints:** Do not document or expose internal IP addresses or private DNS endpoints in public-facing documentation or shared templates.
