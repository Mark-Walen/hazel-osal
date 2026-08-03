#ifndef HAZEL_INCLUDE_SYS_UTIL_H_
#define HAZEL_INCLUDE_SYS_UTIL_H_

#include <stdbool.h>
#include <hazel_wireless/sys/util_macro.h>

/** @brief Cast @p x, a pointer, to an unsigned integer. */
#define POINTER_TO_UINT(x) ((uintptr_t) (x))
/** @brief Cast @p x, an unsigned integer, to a <tt>void*</tt>. */
#define UINT_TO_POINTER(x) ((void *) (uintptr_t) (x))
/** @brief Cast @p x, a pointer, to a signed integer. */
#define POINTER_TO_INT(x)  ((intptr_t) (x))
/** @brief Cast @p x, a signed integer, to a <tt>void*</tt>. */
#define INT_TO_POINTER(x)  ((void *) (intptr_t) (x))

#if !(defined(__CHAR_BIT__) && defined(__SIZEOF_LONG__) && defined(__SIZEOF_LONG_LONG__))
#error Missing required predefined macros for BITS_PER_LONG calculation
#endif

/** Number of bits in a byte. */
#define BITS_PER_BYTE (__CHAR_BIT__)

/** Number of bits in a nibble. */
#define BITS_PER_NIBBLE (__CHAR_BIT__ / 2)

/** Number of nibbles in a byte. */
#define NIBBLES_PER_BYTE (BITS_PER_BYTE / BITS_PER_NIBBLE)

/** Number of bits in a long int. */
#define BITS_PER_LONG (__CHAR_BIT__ * __SIZEOF_LONG__)

/** Number of bits in a long long int. */
#define BITS_PER_LONG_LONG (__CHAR_BIT__ * __SIZEOF_LONG_LONG__)

#undef ARRAY_SIZE

#define ZERO_OR_COMPILE_ERROR(cond) ((int) sizeof(char[1 - 2 * !(cond)]) - 1)

#if defined(__cplusplus)

/* The built-in function used below for type checking in C is not
 * supported by GNU C++.
 */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#else /* __cplusplus */

/**
 * @brief Zero if @p array has an array type, a compile error otherwise
 *
 * This macro is available only from C, not C++.
 */
#define IS_ARRAY(array) \
	ZERO_OR_COMPILE_ERROR( \
		!__builtin_types_compatible_p(__typeof__(array), \
					      __typeof__(&(array)[0])))

/**
 * @brief Number of elements in the given @p array
 *
 * In C++, due to language limitations, this will accept as @p array
 * any type that implements <tt>operator[]</tt>. The results may not be
 * particularly meaningful in this case.
 *
 * In C, passing a pointer as @p array causes a compile error.
 */
#define ARRAY_SIZE(array) \
	((size_t) (IS_ARRAY(array) + (sizeof(array) / sizeof((array)[0]))))

#endif /* __cplusplus */

/**
 * @brief Whether @p ptr is an element of @p array
 *
 * This macro can be seen as a slightly stricter version of @ref PART_OF_ARRAY
 * in that it also ensures that @p ptr is aligned to an array-element boundary
 * of @p array.
 *
 * In C, passing a pointer as @p array causes a compile error.
 *
 * @param array the array in question
 * @param ptr the pointer to check
 *
 * @return 1 if @p ptr is part of @p array, 0 otherwise
 */
#define IS_ARRAY_ELEMENT(array, ptr)                                                               \
	((ptr) && POINTER_TO_UINT(array) <= POINTER_TO_UINT(ptr) &&                          \
	 POINTER_TO_UINT(ptr) < POINTER_TO_UINT(&(array)[ARRAY_SIZE(array)]) &&                    \
	 (POINTER_TO_UINT(ptr) - POINTER_TO_UINT(array)) % sizeof((array)[0]) == 0)

/**
 * @brief Index of @p ptr within @p array
 *
 * With `CONFIG_ASSERT=y`, this macro will trigger a runtime assertion
 * when @p ptr does not fall into the range of @p array or when @p ptr
 * is not aligned to an array-element boundary of @p array.
 *
 * In C, passing a pointer as @p array causes a compile error.
 *
 * @param array the array in question
 * @param ptr pointer to an element of @p array
 *
 * @return the array index of @p ptr within @p array, on success
 */
#define ARRAY_INDEX(array, ptr)                                                                    \
	({                                                                                         \
		__ASSERT_NO_MSG(IS_ARRAY_ELEMENT(array, ptr));                                     \
		(__typeof__((array)[0]) *)(ptr) - (array);                                         \
	})

/**
 * @brief Check if a pointer @p ptr lies within @p array.
 *
 * In C but not C++, this causes a compile error if @p array is not an array
 * (e.g. if @p ptr and @p array are mixed up).
 *
 * @param array an array
 * @param ptr a pointer
 * @return 1 if @p ptr is part of @p array, 0 otherwise
 */
#define PART_OF_ARRAY(array, ptr)                                                                  \
	((ptr) && POINTER_TO_UINT(array) <= POINTER_TO_UINT(ptr) &&                                \
	 POINTER_TO_UINT(ptr) < POINTER_TO_UINT(&(array)[ARRAY_SIZE(array)]))

/**
 * @brief Array-index of @p ptr within @p array, rounded down
 *
 * This macro behaves much like @ref ARRAY_INDEX with the notable
 * difference that it accepts any @p ptr in the range of @p array rather than
 * exclusively a @p ptr aligned to an array-element boundary of @p array.
 *
 * With `CONFIG_ASSERT=y`, this macro will trigger a runtime assertion
 * when @p ptr does not fall into the range of @p array.
 *
 * In C, passing a pointer as @p array causes a compile error.
 *
 * @param array the array in question
 * @param ptr pointer to an element of @p array
 *
 * @return the array index of @p ptr within @p array, on success
 */
#define ARRAY_INDEX_FLOOR(array, ptr)                                                              \
	({                                                                                         \
		__ASSERT_NO_MSG(PART_OF_ARRAY(array, ptr));                                        \
		(POINTER_TO_UINT(ptr) - POINTER_TO_UINT(array)) / sizeof((array)[0]);              \
	})

/**
 * @brief Get a pointer to a structure containing the element
 *
 * Example:
 *
 *	struct foo {
 *		int bar;
 *	};
 *
 *	struct foo my_foo;
 *	int *ptr = &my_foo.bar;
 *
 *	struct foo *container = CONTAINER_OF(ptr, struct foo, bar);
 *
 * Above, @p container points at @p my_foo.
 *
 * @param ptr pointer to a structure element
 * @param type name of the type that @p ptr is an element of
 * @param field the name of the field within the struct @p ptr points to
 * @return a pointer to the structure that contains @p ptr
 */
#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, field) \
	((type *)(((char *)(ptr)) - offsetof(type, field)))
#endif

/**
 * @brief Value of @p x rounded up to the next multiple of @p align,
 *        which must be a power of 2.
 */
#define ROUND_UP(x, align)                                   \
	(((unsigned long)(x) + ((unsigned long)(align) - 1)) & \
	 ~((unsigned long)(align) - 1))

/**
 * @brief Value of @p x rounded down to the previous multiple of @p
 *        align, which must be a power of 2.
 */
#define ROUND_DOWN(x, align)                                 \
	((unsigned long)(x) & ~((unsigned long)(align) - 1))

/** @brief Value of @p x rounded up to the next word boundary. */
#define WB_UP(x) ROUND_UP(x, sizeof(void *))

/** @brief Value of @p x rounded down to the previous word boundary. */
#define WB_DN(x) ROUND_DOWN(x, sizeof(void *))

/**
 * @brief Divide and round up.
 *
 * Example:
 * @code{.c}
 * DIV_ROUND_UP(1, 2); // 1
 * DIV_ROUND_UP(3, 2); // 2
 * @endcode
 *
 * @param n Numerator.
 * @param d Denominator.
 *
 * @return The result of @p n / @p d, rounded up.
 */
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

/**
 * @brief Divide and round to the nearest integer.
 *
 * Example:
 * @code{.c}
 * DIV_ROUND_CLOSEST(5, 2); // 3
 * DIV_ROUND_CLOSEST(5, -2); // -3
 * DIV_ROUND_CLOSEST(5, 3); // 2
 * @endcode
 *
 * @param n Numerator.
 * @param d Denominator.
 *
 * @return The result of @p n / @p d, rounded to the nearest integer.
 */
#define DIV_ROUND_CLOSEST(n, d)                                                                    \
	(((((__typeof__(n))-1) < 0) && (((__typeof__(d))-1) < 0) && ((n) < 0) ^ ((d) < 0))         \
		 ? ((n) - ((d) / 2)) / (d)                                                         \
		 : ((n) + ((d) / 2)) / (d))

/**
 * @cond INTERNAL_HIDDEN
 */
#define Z_INTERNAL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define Z_INTERNAL_MIN(a, b) (((a) < (b)) ? (a) : (b))

#define _minmax_unique(op, a, b, ua, ub) ({ \
		__typeof__(a) ua = (a);     \
		__typeof__(b) ub = (b);     \
		op(ua, ub);                 \
	})

#define _minmax_cnt(op, a, b, cnt) \
	_minmax_unique(op, a, b, UTIL_CAT(_value_a_, cnt), UTIL_CAT(_value_b_, cnt))

#define _minmax3_unique(op, a, b, c, ua, ub, uc) ({ \
		__typeof__(a) ua = (a);             \
		__typeof__(b) ub = (b);             \
		__typeof__(c) uc = (c);             \
		op(ua, op(ub, uc));                 \
	})

#define _minmax3_cnt(op, a, b, c, cnt)            \
	_minmax3_unique(op, a, b, c,              \
			UTIL_CAT(_value_a_, cnt), \
			UTIL_CAT(_value_b_, cnt), \
			UTIL_CAT(_value_c_, cnt))
/**
 * @endcond
 */

#ifndef MAX
/**
 * @brief Obtain the maximum of two values.
 *
 * @note Arguments are evaluated twice. Use @ref max for a single evaluation
 * version.
 *
 * @param a First value.
 * @param b Second value.
 *
 * @returns Maximum value of @p a and @p b.
 */
#define MAX(a, b) Z_INTERNAL_MAX(a, b)
#endif

#ifndef __cplusplus
/** @brief Return larger value of two provided expressions.
 *
 * Macro ensures that expressions are evaluated only once.
 *
 * @note Macro has limited usage compared to the standard macro as it cannot be
 *	 used:
 *	 - to generate constant integer, e.g. __aligned(max(4,5))
 *	 - static variable, e.g. array like static uint8_t array[max(...)];
 */
#define max(a, b) _minmax_cnt(Z_INTERNAL_MAX, a, b, __COUNTER__)
#endif

/** @brief Return larger value of three provided expressions.
 *
 * Macro ensures that expressions are evaluated only once. See @ref max for
 * macro limitations.
 */
#define max3(a, b, c) _minmax3_cnt(Z_INTERNAL_MAX, a, b, c, __COUNTER__)

#ifndef MIN
/**
 * @brief Obtain the minimum of two values.
 *
 * @note Arguments are evaluated twice. Use @ref min for a single evaluation
 * version.
 *
 * @param a First value.
 * @param b Second value.
 *
 * @returns Minimum value of @p a and @p b.
 */
#define MIN(a, b) Z_INTERNAL_MIN(a, b)
#endif

#ifndef __cplusplus
/** @brief Return smaller value of two provided expressions.
 *
 * Macro ensures that expressions are evaluated only once. See @ref max for
 * macro limitations.
 */
#define min(a, b) _minmax_cnt(Z_INTERNAL_MIN, a, b, __COUNTER__)
#endif

/** @brief Return smaller value of three provided expressions.
 *
 * Macro ensures that expressions are evaluated only once. See @ref max for
 * macro limitations.
 */
#define min3(a, b, c) _minmax3_cnt(Z_INTERNAL_MIN, a, b, c, __COUNTER__)


#ifndef MAX_FROM_LIST
/**
 * @brief Returns the maximum of a single value (base case).
 * @param a The value.
 * @returns The value `a`.
 */
#define Z_MAX_1(a) a

/**
 * @brief Returns the maximum of two values.
 *
 * @note Arguments are evaluated multiple times.
 *
 * @param a First value.
 * @param b Second value.
 * @returns Maximum value of @p a and @p b.
 */
#define Z_MAX_2(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Returns the maximum of three values.
 * @note Arguments may be evaluated multiple times.
 * @param a First value.
 * @param b Second value.
 * @param c Third value.
 * @returns Maximum value of @p a, @p b, and @p c.
 */
#define Z_MAX_3(a, b, c) Z_MAX_2(a, Z_MAX_2(b, c))

/**
 * @brief Returns the maximum of four values.
 * @note Arguments may be evaluated multiple times.
 * @param a First value.
 * @param b Second value.
 * @param c Third value.
 * @param d Fourth value.
 * @returns Maximum value of @p a, @p b, @p c, and @p d.
 */
#define Z_MAX_4(a, b, c, d) Z_MAX_2(Z_MAX_2(a, b), Z_MAX_2(c, d))

/**
 * @brief Returns the maximum of five values.
 * @note Arguments may be evaluated multiple times.
 */
#define Z_MAX_5(a, b, c, d, e) Z_MAX_2(Z_MAX_4(a, b, c, d), e)

/**
 * @brief Returns the maximum of six values.
 * @note Arguments may be evaluated multiple times.
 */
#define Z_MAX_6(a, b, c, d, e, f) Z_MAX_2(Z_MAX_5(a, b, c, d, e), f)

/**
 * @brief Returns the maximum of seven values.
 * @note Arguments may be evaluated multiple times.
 */
#define Z_MAX_7(a, b, c, d, e, f, g) Z_MAX_2(Z_MAX_6(a, b, c, d, e, f), g)

/**
 * @brief Returns the maximum of eight values.
 * @note Arguments may be evaluated multiple times.
 */
#define Z_MAX_8(a, b, c, d, e, f, g, h) Z_MAX_2(Z_MAX_7(a, b, c, d, e, f, g), h)

/**
 * @brief Returns the maximum of nine values.
 * @note Arguments may be evaluated multiple times.
 */
#define Z_MAX_9(a, b, c, d, e, f, g, h, i) Z_MAX_2(Z_MAX_8(a, b, c, d, e, f, g, h), i)

/**
 * @brief Returns the maximum of ten values.
 * @note Arguments may be evaluated multiple times.
 */
#define Z_MAX_10(a, b, c, d, e, f, g, h, i, j) Z_MAX_2(Z_MAX_9(a, b, c, d, e, f, g, h, i), j)

/**
 * @brief Helper macro to select the correct MAX_N macro.
 *
 * This macro uses the argument-counting trick to pick the correct
 * `Z_MAX_N` macro name from the arguments provided to `MAX_FROM_LIST`.
 * The 10th argument (or 11th including `NAME`) effectively becomes the
 * macro name to use.
 *
 * @param _1 Positional argument 1.
 * @param _2 Positional argument 2.
 * @param _3 Positional argument 3.
 * @param _4 Positional argument 4.
 * @param _5 Positional argument 5.
 * @param _6 Positional argument 6.
 * @param _7 Positional argument 7.
 * @param _8 Positional argument 8.
 * @param _9 Positional argument 9.
 * @param _10 Positional argument 10.
 * @param NAME The macro name to be selected.
 * @param ... Additional arguments.
 * @returns The selected macro name `NAME`.
 */
#define Z_GET_MAX_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, NAME, ...) NAME

/**
 * @brief Finds the maximum value from a list of 1 to 10 arguments.
 *
 * Dispatches to the appropriate internal `Z_MAX_N` macro based on the number of
 * arguments provided.
 *
 * Example Usage:
 *   MAX_FROM_LIST(1, 5, 2)
 *   MAX_FROM_LIST(10)
 *
 * @note Arguments may be evaluated multiple times by the underlying
 *       `Z_MAX_N` macros. Avoid expressions with side effects.
 *
 * @param ... A list of 1 to 10 values to compare.
 * @returns The maximum value among the arguments.
 */
#define MAX_FROM_LIST(...)                                                                         \
	Z_GET_MAX_MACRO(__VA_ARGS__, Z_MAX_10, Z_MAX_9, Z_MAX_8, Z_MAX_7, Z_MAX_6, Z_MAX_5,        \
			Z_MAX_4, Z_MAX_3, Z_MAX_2, Z_MAX_1)(__VA_ARGS__)
#endif

#ifndef CLAMP
/**
 * @brief Clamp a value to a given range.
 *
 * @note Arguments are evaluated multiple times. Use @ref clamp for a single
 * evaluation version.
 *
 * @param val Value to be clamped.
 * @param low Lowest allowed value (inclusive).
 * @param high Highest allowed value (inclusive).
 *
 * @returns Clamped value.
 */
#define CLAMP(val, low, high) (((val) <= (low)) ? (low) : Z_INTERNAL_MIN(val, high))
#endif

#ifndef __cplusplus
/** @brief Return a value clamped to a given range.
 *
 * Macro ensures that expressions are evaluated only once. See @ref max for
 * macro limitations.
 */
#define clamp(val, low, high) ({                                               \
		/* random suffix to avoid naming conflict */                   \
		__typeof__(val) _value_val_ = (val);                           \
		__typeof__(low) _value_low_ = (low);                           \
		__typeof__(high) _value_high_ = (high);                        \
		(_value_val_ < _value_low_)  ? _value_low_ :                   \
		(_value_val_ > _value_high_) ? _value_high_ :                  \
					       _value_val_;                    \
	})
#endif

/**
 * @brief Checks if a value is within range.
 *
 * @note @p val is evaluated twice.
 *
 * @param val Value to be checked.
 * @param min Lower bound (inclusive).
 * @param max Upper bound (inclusive).
 *
 * @retval true If value is within range
 * @retval false If the value is not within range
 */
#define IN_RANGE(val, min, max) ((val) >= (min) && (val) <= (max))

#endif
