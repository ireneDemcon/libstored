/*
 * libstored, a Store for Embedded Debugger.
 * Copyright (C) 2020  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libstored/macros.h>
#include <libstored/compress.h>

#ifdef STORED_HAVE_HEATSHRINK
extern "C" {
#  include <heatshrink_encoder.h>
#  include <heatshrink_decoder.h>
}

static heatshrink_encoder& encoder_(void* e) {
	stored_assert(e);
	return *static_cast<heatshrink_encoder*>(e);
}
#define encoder() (encoder_(m_encoder))

static heatshrink_decoder& decoder_(void* d) {
	stored_assert(d);
	return *static_cast<heatshrink_decoder*>(d);
}
#define decoder() (decoder_(m_decoder))

namespace stored {

CompressLayer::CompressLayer(ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_encoder()
	, m_decoder()
	, m_decodeBufferSize()
{
}

CompressLayer::~CompressLayer() {
	if(m_encoder)
		heatshrink_encoder_free(&encoder());

	if(m_decoder)
		heatshrink_decoder_free(&decoder());
}

void CompressLayer::decode(void* buffer, size_t len) {
	if(!buffer || !len)
		return;

	if(unlikely(!m_decoder))
		if(!(m_decoder = heatshrink_decoder_alloc(DecodeInputBuffer, Window, Lookahead)))
			throw std::bad_alloc();

	m_decodeBufferSize = 0;

	uint8_t* in_buf = static_cast<uint8_t*>(buffer);
	while(len > 0) {
		size_t sunk = 0;
		HSD_sink_res res = heatshrink_decoder_sink(&decoder(), in_buf, len, &sunk);
		stored_assert(res == HSDR_SINK_OK);
		(void)res;
		stored_assert(sunk <= len);
		len -= sunk;
		in_buf += sunk;

		decoderPoll();
	}

	while(heatshrink_decoder_finish(&decoder()) == HSDR_FINISH_MORE)
		decoderPoll();

	base::decode(&m_decodeBuffer[0], m_decodeBufferSize);
}

void CompressLayer::decoderPoll() {
	while(true) {
		m_decodeBuffer.resize(m_decodeBufferSize + 128);
		size_t output_size = 0;
		HSD_poll_res res = heatshrink_decoder_poll(&decoder(), &m_decodeBuffer[m_decodeBufferSize], m_decodeBuffer.size() - m_decodeBufferSize, &output_size);

		m_decodeBufferSize += output_size;
		stored_assert(m_decodeBufferSize <= m_decodeBuffer.size());

		switch(res) {
		case HSDR_POLL_EMPTY:
			return;
		case HSDR_POLL_MORE:
			break;
		default:
			stored_assert(false); // NOLINT(hicpp-static-assert,misc-static-assert)
		}
	}
}

void CompressLayer::encode(void const* buffer, size_t len, bool last) {
	if(!buffer || !len)
		return;

	if(unlikely(!m_encoder))
		if(!(m_encoder = heatshrink_encoder_alloc(Window, Lookahead)))
			throw std::bad_alloc();

	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
	uint8_t* in_buf = (uint8_t*)buffer;

	while(len > 0) {
		size_t sunk = 0;
		HSE_sink_res res = heatshrink_encoder_sink(&encoder(), in_buf, len, &sunk);
		stored_assert(res == HSER_SINK_OK);
		(void)res;
		stored_assert(sunk <= len);
		len -= sunk;
		in_buf += sunk;

		encoderPoll();
	}

	if(last) {
		while(heatshrink_encoder_finish(&encoder()) == HSER_FINISH_MORE)
			encoderPoll();
		base::encode();
		heatshrink_encoder_reset(&encoder());
	}
}

void CompressLayer::encoderPoll() {
	uint8_t out_buf[128];

	while(true) {
		size_t output_size = 0;
		HSE_poll_res res = heatshrink_encoder_poll(&encoder(), out_buf, sizeof(out_buf), &output_size);

		if(output_size > 0)
			base::encode(out_buf, output_size);

		switch(res) {
		case HSER_POLL_EMPTY:
			return;
		case HSER_POLL_MORE:
			break;
		default:
			stored_assert(false); // NOLINT(hicpp-static-assert,misc-static-assert)
		}
	}
}

size_t CompressLayer::mtu() const {
	// This is a stream; we cannot handle limited messages.
	// Use the SegmentationLayer for that.
	stored_assert(base::mtu() == 0);
	return 0;
}

} // namespace
#endif // STORED_HAVE_HEATSHRINK
