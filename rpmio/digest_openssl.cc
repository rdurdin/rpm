#include "system.h"

#include <openssl/evp.h>
#include <rpm/rpmcrypto.h>
#include <rpm/rpmstring.h>

struct DIGEST_CTX_s {
    rpmDigestFlags flags;	/*!< Bit(s) to control digest operation. */
    int algo;			/*!< Used hash algorithm */

    EVP_MD_CTX *md_ctx; /* Digest context (opaque) */

};

/****************************  init   ************************************/

int rpmInitCrypto(void) {
    return 0;
}

int rpmFreeCrypto(void) {
    return 0;
}

/****************************  digest ************************************/

DIGEST_CTX rpmDigestDup(DIGEST_CTX octx)
{
    if (!octx) return NULL;

    DIGEST_CTX nctx = new DIGEST_CTX_s { *octx };

    nctx->md_ctx = EVP_MD_CTX_new();
    if (!nctx->md_ctx) {
        delete nctx;
        return NULL;
    }

    if (!EVP_MD_CTX_copy(nctx->md_ctx, octx->md_ctx)) {
        delete nctx;
        return NULL;
    }

    return nctx;
}

static const EVP_MD *getEVPMD(int hashalgo)
{
    switch (hashalgo) {

    case RPM_HASH_MD5:
        return EVP_md5();

    case RPM_HASH_SHA1:
        return EVP_sha1();

    case RPM_HASH_SHA256:
        return EVP_sha256();

    case RPM_HASH_SHA384:
        return EVP_sha384();

    case RPM_HASH_SHA512:
        return EVP_sha512();

    case RPM_HASH_SHA224:
        return EVP_sha224();

    case RPM_HASH_SHA3_256:
        return EVP_sha3_256();

    case RPM_HASH_SHA3_512:
        return EVP_sha3_512();

    default:
        return EVP_md_null();
    }
}

size_t rpmDigestLength(int hashalgo)
{
    return EVP_MD_size(getEVPMD(hashalgo));
}

DIGEST_CTX rpmDigestInit(int hashalgo, rpmDigestFlags flags)
{
    DIGEST_CTX ctx = new DIGEST_CTX_s {};

    ctx->md_ctx = EVP_MD_CTX_new();
    if (!ctx->md_ctx) {
        delete ctx;
        return NULL;
    }

    const EVP_MD *md = getEVPMD(hashalgo);
    if (md == EVP_md_null()) {
        free(ctx->md_ctx);
        delete ctx;
        return NULL;
    }

    ctx->algo = hashalgo;
    ctx->flags = flags;
    if (!EVP_DigestInit_ex(ctx->md_ctx, md, NULL)) {
        free(ctx->md_ctx);
        delete ctx;
        return NULL;
    }

    return ctx;
}

int rpmDigestUpdate(DIGEST_CTX ctx, const void *data, size_t len)
{
    if (ctx == NULL) return -1;

    EVP_DigestUpdate(ctx->md_ctx, data, len);

    return 0;
}

int rpmDigestFinal(DIGEST_CTX ctx, void ** datap, size_t *lenp, int asAscii)
{
    int ret;
    uint8_t *digest = NULL;
    unsigned int digestlen;

    if (ctx == NULL) return -1;

    digestlen = EVP_MD_CTX_size(ctx->md_ctx);
    digest = (uint8_t *)xcalloc(digestlen, sizeof(*digest));

    ret = EVP_DigestFinal_ex(ctx->md_ctx, digest, &digestlen);
    if (ret != 1) goto done;

    if (!asAscii) {
        /* Raw data requested */
        if (lenp) *lenp = digestlen;
        if (datap) {
            *datap = digest;
            digest = NULL;
        }
    }

    else {
        /* ASCII requested */
        if (lenp) *lenp = (2*digestlen) + 1;
        if (datap) {
            const uint8_t * s = (const uint8_t *) digest;
            *datap = rpmhex(s, digestlen);
        }
    }

    ret = 1;

done:
    if (digest) {
        /* Zero the digest, just in case it's sensitive */
        memset(digest, 0, digestlen);
        free(digest);
    }

    EVP_MD_CTX_free(ctx->md_ctx);
    delete ctx;

    if (ret != 1) {
        return -1;
    }

    return 0;
}

