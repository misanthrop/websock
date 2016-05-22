#ifndef WEBSOCK_HPP
#define WEBSOCK_HPP

#ifndef WEBSOCK_NO_STL
#include <functional>
#endif
#include "buffer.h"
#include "websock.h"

namespace websock
{
	#ifndef WEBSOCK_NO_STL
	template<class t> using function = std::function<t>;
	#endif
	using Handshake = websock_handshake;
	using Message = websock_message;

	enum Error
	{
		UnexpectedDisconnect,
		ReadFailed,
		WriteFailed,
		HandshakeFailed,
		UnmaskedMessage,
		TooLongMessage
	};

	constexpr const char *ErrorText[] =
	{
		[UnexpectedDisconnect] = "unexpected disconnect",
		[ReadFailed] = "read failed",
		[WriteFailed] = "write failed",
		[HandshakeFailed] = "handshake failed",
		[UnmaskedMessage] = "received unmasked message",
		[TooLongMessage] = "received too long message"
	};

	struct Connection
	{
		Buffer in, out;
		bool connected = true;
		bool established = false;
		bool allowUnmasked = false;

		/// should return the number of bytes read, -1 on error and 0 on disconnect (like POSIX recv)
		function<ptrdiff_t(char*, size_t)> read;

		/// should return the number of bytes sent and -1 on error (like POSIX send)
		function<ptrdiff_t(const char*, size_t)> write;

		function<void(const Handshake&)> onConnect;
		function<void(const Message&)> onMessage;
		function<void(Error)> onError;
		function<void()> onClose;

		Connection(size_t maxMsgLen = 2048) : in(maxMsgLen), out(maxMsgLen) {}

		void close()
		{
			if(!connected) return;
			if(onClose) onClose();
			connected = false;
		}

		void error(Error code)
		{
			if(onError) onError(code);
			close();
		}

		void process()
		{
			if(!connected) return;
			if(read)
			{
				in.shrink();
				if(!in.spaceLeft())
					return error(established ? TooLongMessage : HandshakeFailed);
				ptrdiff_t available = read(in.end(), in.spaceLeft());
				if(available > 0)
					in.write(0, available);
				else
					return error(available ? ReadFailed : UnexpectedDisconnect);
			}
			if(!connected) return;
			if(!established)
			{
				Handshake hs;
				size_t n = websocket_parse_handshake(in.begin(), in.size(), &hs);
				if(!n) return;
				in.skip(n);
				out.setEnd(websock_accept_handshake(out.end(), hs.sec_key, hs.sec_key_len));
				established = true;
				if(onConnect) onConnect(hs);
			}
			while(connected)
			{
				Message msg;
				size_t n = websocket_decode_message(in.begin(), in.size(), &msg);
				if(!n) return;
				in.skip(n);
				if(!allowUnmasked && !msg.masked)
					return error(UnmaskedMessage);
				switch(msg.opcode)
				{
					case websock_opcode_ping:
						send(msg.data, msg.len, websock_opcode_pong);
						break;
					case websock_opcode_close:
						return close();
					default:
						if(onMessage) onMessage(msg);
				}
			}
		}

		void flush()
		{
			if(!connected || !out.size()) return;
			ptrdiff_t sent = write(out.begin(), out.size());
			if(sent == -1) return error(WriteFailed);
			out.skip(sent);
			out.shrink();
		}

		void send(const char* data, size_t len, uint8_t opcode = websock_opcode_binary)
		{
			out.reserve(len + 8);
			out.setEnd(websocket_write_message(out.end(), data, len, opcode | websock_final_mask));
		}
	};
}

#endif
