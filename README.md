# BSGuardEncryption

This Unreal Engine plugin transparently encrypts asset files using AES-256-GCM. A custom platform file intercepts reads and writes of `.uasset`, `.uexp` and `.ubulk` files so they are stored encrypted on disk.

## Setup
1. Build the plugin with your project and enable it.
2. In **Project Settings > GuardEncryption**, enter your key in the format `<Base64Key>|<Expiration>|<UserID>`.
   - Example: `AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=|2025-12-31T23:59:59Z|UserA`
3. Restart the editor to apply the key.

## Usage
* When a valid key is set, saving assets automatically encrypts them.
* Loading an encrypted asset requires the same user ID and a non-expired key.
* Expired keys will be rejected; provide a new key string to continue using encrypted content.

## Build
The plugin depends on OpenSSL which is included with Unreal Engine. Add the plugin to your `Plugins` directory and regenerate project files before compiling.
