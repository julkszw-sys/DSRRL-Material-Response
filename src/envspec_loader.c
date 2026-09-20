/*
 * DSRRL Material Response 1.45
 * External PackedGI EnvSpec loader
 *
 * Readable C form of the loader contract used by the shipping addon.
 * The release binary uses an equivalent fixed Win64 implementation stored
 * as bytes in reference/loader_bytes.hex, so this file is intended to make
 * the behaviour easy to inspect rather than to reproduce compiler output.
 *
 * The loader:
 *   - builds one fixed path below the game directory
 *   - opens the PackedGI file read-only
 *   - requires the exact expected size
 *   - temporarily makes the addon's reserved resource area writable
 *   - reads the complete resource
 *   - validates SHA-256 with Windows CryptoAPI
 *   - restores the original memory protection
 *   - enables the EnvSpec path only after validation succeeds
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#pragma comment(lib, "advapi32.lib")

#define DSRRL_ENVSPEC_PACK_SIZE ((DWORD)33619968u)

static const wchar_t kEnvSpecRelativePath[] =
    L"DSRRL\\EnvSpec\\PackedGI\\PTDE_GI_ENVSPEC_PACK_RGBA.bin";

static const uint8_t kExpectedSha256[32] = {
    0xc1,0x6c,0x3f,0xd7,0x5b,0xcf,0x34,0xf3,
    0xcc,0x07,0x5d,0xa6,0xda,0x1a,0xd1,0x0c,
    0x94,0x40,0xee,0x4a,0x3c,0xa5,0x80,0xfe,
    0xe7,0x7f,0x74,0xd0,0x7a,0x2c,0xe4,0xc3
};

typedef struct DSRRL_EnvSpecLoaderState {
    void *destination;
    SIZE_T destination_capacity;
    bool ready;
} DSRRL_EnvSpecLoaderState;

static bool build_pack_path(wchar_t out_path[MAX_PATH * 2])
{
    DWORD n = GetModuleFileNameW(NULL, out_path, MAX_PATH * 2);
    if (n == 0 || n >= MAX_PATH * 2)
        return false;

    wchar_t *slash = wcsrchr(out_path, L'\\');
    if (slash == NULL)
        return false;

    slash[1] = L'\0';

    const size_t have = wcslen(out_path);
    const size_t need = wcslen(kEnvSpecRelativePath);
    if (have + need + 1 >= MAX_PATH * 2)
        return false;

    memcpy(out_path + have, kEnvSpecRelativePath,
           (need + 1) * sizeof(wchar_t));
    return true;
}

static bool sha256_buffer(const void *data, DWORD size, uint8_t out_hash[32])
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    DWORD hash_size = 32;
    bool ok = false;

    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
        goto done;
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash))
        goto done;
    if (!CryptHashData(hash, (const BYTE *)data, size, 0))
        goto done;
    if (!CryptGetHashParam(hash, HP_HASHVAL, out_hash, &hash_size, 0))
        goto done;

    ok = (hash_size == 32);

done:
    if (hash != 0)
        CryptDestroyHash(hash);
    if (provider != 0)
        CryptReleaseContext(provider, 0);
    return ok;
}

bool dsrrl_load_external_envspec_pack(DSRRL_EnvSpecLoaderState *state)
{
    wchar_t path[MAX_PATH * 2];
    LARGE_INTEGER file_size;
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD old_protect = 0;
    DWORD ignored_protect = 0;
    DWORD bytes_read = 0;
    uint8_t digest[32];
    bool protection_changed = false;
    bool success = false;

    if (state == NULL || state->destination == NULL ||
        state->destination_capacity < DSRRL_ENVSPEC_PACK_SIZE)
        return false;

    state->ready = false;

    if (!build_pack_path(path))
        goto done;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        goto done;

    if (!GetFileSizeEx(file, &file_size))
        goto done;
    if (file_size.QuadPart != DSRRL_ENVSPEC_PACK_SIZE)
        goto done;

    if (!VirtualProtect(state->destination, DSRRL_ENVSPEC_PACK_SIZE,
                        PAGE_READWRITE, &old_protect))
        goto done;
    protection_changed = true;

    if (!ReadFile(file, state->destination, DSRRL_ENVSPEC_PACK_SIZE,
                  &bytes_read, NULL))
        goto done;
    if (bytes_read != DSRRL_ENVSPEC_PACK_SIZE)
        goto done;

    if (!sha256_buffer(state->destination, DSRRL_ENVSPEC_PACK_SIZE, digest))
        goto done;

    if (memcmp(digest, kExpectedSha256, sizeof(kExpectedSha256)) != 0)
        goto done;

    state->ready = true;
    success = true;

done:
    if (protection_changed) {
        VirtualProtect(state->destination, DSRRL_ENVSPEC_PACK_SIZE,
                       old_protect, &ignored_protect);
    }
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);

    return success;
}

void *dsrrl_envspec_state_if_ready(const DSRRL_EnvSpecLoaderState *state,
                                   void *bridge_state)
{
    return (state != NULL && state->ready) ? bridge_state : NULL;
}
