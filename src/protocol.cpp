#include <libstored/protocol.h>

#include <unistd.h>

namespace stored {

ProtocolLayer::~ProtocolLayer() {
	if(up() && up()->down() == this)
		up()->setDown(down());
	if(down() && down()->up() == this)
		down()->setUp(up());
}

TerminalLayer::TerminalLayer(int nonDebugDecodeFd, int encodeFd, ProtocolLayer* up, ProtocolLayer* down)
	: base(up, down)
	, m_nonDebugDecodeFd(nonDebugDecodeFd)
	, m_encodeFd(encodeFd)
	, m_decodeState(StateNormal)
	, m_encodeState()
{}

TerminalLayer::~TerminalLayer()
#if __cplusplus < 201103L
{}
#else
	= default;
#endif

void TerminalLayer::decode(void* buffer, size_t len) {
	size_t nonDebugOffset = m_decodeState < StateDebug ? 0 : len;
	
	for(size_t i = 0; i < len; i++) {
		char c = (static_cast<char*>(buffer))[i];

		switch(m_decodeState) {
		case StateNormal:
			if(c == Esc)
				m_decodeState = StateNormalEsc;
			break;
		case StateNormalEsc:
			if(c == EscStart) {
				nonDebugDecode(static_cast<char*>(buffer) + nonDebugOffset, i - nonDebugOffset - 1); // Also skip the ESC
				m_decodeState = StateDebug;
				nonDebugOffset = len;
			} else
				m_decodeState = StateNormal;
			break;
		case StateDebug:
			if(c == Esc)
				m_decodeState = StateDebugEsc;
			else
				m_buffer.push_back(c);
			break;
		case StateDebugEsc:
			if(c == EscEnd) {
				base::decode(&m_buffer[0], m_buffer.size());
				m_decodeState = StateNormal;
				m_buffer.clear();
				nonDebugOffset = i + 1;
			} else {
				m_decodeState = StateDebug;
				m_buffer.push_back((char)Esc);
				m_buffer.push_back(c);
			}
			break;
		}
	}

	if(nonDebugOffset < len)
		nonDebugDecode(static_cast<char*>(buffer) + nonDebugOffset, len - nonDebugOffset);
}

void TerminalLayer::nonDebugDecode(void* buffer, size_t len) {
	writeToFd(m_nonDebugDecodeFd, buffer, len);
}

void TerminalLayer::writeToFd(int fd, void const* buffer, size_t len) {
	if(fd < 0)
		return;

	ssize_t res = 0;
	for(size_t i = 0; res >= 0 && i < len; i += (size_t)res)
		res = write(fd, static_cast<char const*>(buffer) + i,
#ifdef STORED_OS_WINDOWS
				(unsigned int)
#endif
				(len - i));
}

void TerminalLayer::encode(void* buffer, size_t len, bool last) {
	encodeStart();
	writeToFd(m_encodeFd, buffer, len);
	base::encode(buffer, len, false);
	if(last)
		encodeEnd();
}

void TerminalLayer::encode(void const* buffer, size_t len, bool last) {
	encodeStart();
	writeToFd(m_encodeFd, buffer, len);
	base::encode(buffer, len, false);
	if(last)
		encodeEnd();
}

void TerminalLayer::encodeStart() {
	if(m_encodeState)
		return;
	
	m_encodeState = true;
	char start[2] = {Esc, EscStart};
	writeToFd(m_encodeFd, start, sizeof(start));
	base::encode((void*)start, sizeof(start), false);
}

void TerminalLayer::encodeEnd() {
	if(!m_encodeState)
		return;

	char end[2] = {Esc, EscEnd};
	writeToFd(m_encodeFd, end, sizeof(end));
	base::encode((void*)end, sizeof(end), true);
	m_encodeState = false;
}

} // namespace

