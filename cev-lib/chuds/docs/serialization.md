# CHUDS message bodies

A typed message body goes on the bus by copying the struct's raw bytes (`std::bit_cast`) and is
rebuilt byte-for-byte on the receiving node. So a body's exact in-memory representation _is_ the
wire format. That is fast and simple, but it means not every struct is safe to send.

## Requirements

`make_message<T>` / `body_as<T>` accept a body only if all of these hold:

1. Opt-in marker: the struct declares `static constexpr bool chuds_wire_body = true;`.
2. Trivially copyable and standard-layout.
3. `sizeof(T) <= 62` (fits the CAN-FD payload after the 2-byte header).

Plus two invariants every body must uphold, each backed by an assert:

- No padding. Each body carries `static_assert(sizeof(T) == <sum of field bytes>)`. If the
  compiler inserts padding, `sizeof` exceeds the total and it fails to compile. Padding bytes are
  indeterminate and would leak onto the bus.
- Little-endian host with IEEE 754 floats, asserted once in `message.hpp`
  (`std::endian::native == std::endian::little`, `std::numeric_limits<float>::is_iec559`).

## Why the marker

The compiler cannot check "safe to serialize" generically. There is no standard trait for "no
padding" or "no pointer members" without C++26 static reflection, which the firmware toolchain
(arm-none-eabi-gcc) does not have. `std::has_unique_object_representations` is close but rejects
floats (the joulemeter sends four), so it is unusable here.

So a body declares itself safe to send by opting in. Anything not marked, such as a
`std::string_view`, a struct with a pointer member, or a stray trivially-copyable type, is
rejected at compile time.
The author owns the parts the compiler cannot see: no pointers, no padding (the `sizeof` assert
proves this), fixed-width fields.

## What is not checked

`body_as<T>` copies whatever bytes arrived into `T` and does no field validation. The sender may be
buggy or built from an older struct, so any field can hold any byte. Callers check values
themselves.

- **Integers and floats:** every byte pattern is a valid value, so a bad value is only a wrong
  number. Range-check it where it matters.
- **Enums with a fixed underlying type:** every underlying value is allowed, including ones with no
  name. A `switch` over one needs a `default` that ignores unknown values.
- **`bool`:** only the bytes `0x00` and `0x01` are valid. Any other byte, such as `0x02`, is
  undefined behavior: the compiler may treat the same field as true in one `if` and false in the
  next. Never put a `bool` in a body. Use a `std::uint8_t` and test `!= 0`, or pack flags into a
  bitmask like `BodyState::bits`.

## Adding a body

1. Use only fixed-width integers (`std::uint16_t`, ...), IEEE floats, or fixed-underlying-type
   enums. No pointers, no `std::string`/`std::string_view`/`std::vector`/`std::span`, and no
   `bool` (see above).
2. Order fields so there is no padding, then `static_assert(sizeof(T) == N)`.
3. Add `static constexpr bool chuds_wire_body = true;` inside the struct. It is `static`, so it
   takes up no bytes.

## Future

Once C++26 reflection reaches the arm toolchain, the marker and the per-body `sizeof` assert can
be replaced by a generic `is_wire_safe<T>()` that walks the members and checks layout directly.
Until then the marker is the bridge.
