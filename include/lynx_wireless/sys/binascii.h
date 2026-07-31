#ifndef LYNX_INCLUDE_SYS_BINASCII_H_
#define LYNX_INCLUDE_SYS_BINASCII_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdint.h>

/* ============================================================================
 * Error codes
 * ========================================================================== */

#ifndef BINASCII_OK
#define BINASCII_OK            (0)
#endif

#ifndef BINASCII_EINVAL
#define BINASCII_EINVAL        (-22)
#endif

#ifndef BINASCII_ENOMEM
#define BINASCII_ENOMEM        (-12)
#endif

#ifndef BINASCII_EILSEQ
#define BINASCII_EILSEQ        (-84)
#endif

/* ============================================================================
 * Config
 * ========================================================================== */

typedef enum {
    BINASCII_HEX_LOWERCASE = 0,
    BINASCII_HEX_UPPERCASE = 1
} binascii_hex_case_t;

/* ============================================================================
 * Helpers
 * ========================================================================== */

/**
 * @brief Required hex buffer size for encoding.
 *
 * Includes NULL terminator.
 */
#define BINASCII_HEX_ENCODE_SIZE(bin_len) \
    (((size_t)(bin_len) * 2u) + 1u)

/**
 * @brief Maximum decoded binary size from hex.
 *
 * Input must be even length.
 */
#define BINASCII_HEX_DECODE_SIZE(hex_len) \
    ((size_t)(hex_len) / 2u)

/* ============================================================================
 * API
 * ========================================================================== */

/**
 * @brief Encode binary data into hex string.
 *
 * Result is NULL terminated.
 *
 * @param src         Binary input.
 * @param src_len     Input length.
 * @param dst         Output hex string.
 * @param dst_size    Output buffer size.
 * @param letter_case Lower/upper hex letters.
 *
 * @return
 * >0  encoded hex length
 * <0  error code
 */
int binascii_hex_encode(
    const uint8_t *src,
    size_t src_len,
    char *dst,
    size_t dst_size,
    binascii_hex_case_t letter_case);

/**
 * @brief Decode hex string into binary.
 *
 * @param src         Hex input.
 * @param src_len     Hex length.
 * @param dst         Binary output.
 * @param dst_size    Output buffer size.
 *
 * @return
 * >0  decoded byte count
 * <0  error code
 */
int binascii_hex_decode(
    const char *src,
    size_t src_len,
    uint8_t *dst,
    size_t dst_size);

/* ============================================================================
 * Compatibility aliases
 * ========================================================================== */

/**
 * @brief Converts binary data to a lowercase hexadecimal string.
 *
 * @param bin Pointer to the input binary buffer.
 * @param hex Pointer to the output hexadecimal string buffer.
 * @param bin_len Length of the binary data in bytes.
 * @return 0 on success, or a non-zero error code on failure.
 */
#define binascii_hexlify(bin, hex, bin_len) \
    binascii_hex_encode((bin), (bin_len), (hex), BINASCII_HEX_ENCODE_SIZE(bin_len), BINASCII_HEX_LOWERCASE)

/**
 * @brief Alias for binascii_hexlify.
 *
 * Converts binary data to a hexadecimal string (binary-to-ascii).
 *
 * @param bin Pointer to the input binary buffer.
 * @param hex Pointer to the output hexadecimal string buffer.
 * @param bin_len Length of the binary data in bytes.
 * @return 0 on success, or a non-zero error code on failure.
 */
#define binascii_b2a_hex(bin, hex, bin_len) \
    binascii_hexlify((bin), (hex), (bin_len))

/**
 * @brief Converts a hexadecimal string to binary data.
 *
 * @param hex Pointer to the input hexadecimal string.
 * @param bin Pointer to the output binary buffer.
 * @param hex_len Length of the hexadecimal string.
 * @return 0 on success, or a non-zero error code on failure (e.g., invalid hex characters).
 */
#define binascii_unhexlify(hex, bin, hex_len) \
    binascii_hex_decode((hex), (hex_len), (bin), BINASCII_HEX_DECODE_SIZE(hex_len))

/**
 * @brief Alias for binascii_unhexlify.
 *
 * Converts a hexadecimal string to binary data (ascii-to-binary).
 *
 * @param hex Pointer to the input hexadecimal string.
 * @param bin Pointer to the output binary buffer.
 * @param hex_len Length of the hexadecimal string.
 * @return 0 on success, or a non-zero error code on failure (e.g., invalid hex characters).
 */
#define binascii_a2b_hex(hex, bin, hex_len) \
    binascii_unhexlify((hex), (bin), (hex_len))


#ifdef __cplusplus
}
#endif

#endif
