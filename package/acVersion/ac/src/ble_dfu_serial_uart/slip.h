#ifndef SLIP_H__
#define SLIP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/** @brief Status information that is used while receiving and decoding a packet. */
typedef enum
{
  SLIP_STATE_DECODING, //!< Ready to receive the next byte.
  SLIP_STATE_ESC_RECEIVED, //!< An ESC byte has been received and the next byte must be decoded differently.
  SLIP_STATE_CLEARING_INVALID_PACKET //!< The received data is invalid and transfer must be restarted.
} slip_read_state_t;

  /** @brief Representation of a SLIP packet. */
typedef struct
{
  slip_read_state_t   state; //!< Current state of the packet (see @ref slip_read_state_t).

  uint8_t             * p_buffer; //!< Decoded data.
  uint32_t            current_index; //!< Current length of the packet that has been received.
  uint32_t            buffer_len; //!< Size of the buffer that is available.
} slip_t;

bool slip_encode(uint8_t * p_output,  uint8_t * p_input, uint32_t input_length, uint32_t * p_output_buffer_length);
bool slip_decode_add_byte(slip_t * p_slip, uint8_t c);



#endif //SLIP_H__
