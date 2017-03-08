#include "AuthCrypt.h"
#include "Hmac.h"
#include "BigNumber.h"

AuthCrypt::AuthCrypt()
{
	_initialized = false;
}

void AuthCrypt::Init(BigNumber *bn)
{
	_send_i = _send_j = _recv_i = _recv_j = 0;

	const size_t len = 40;

	_key.resize(len);
	auto const key = bn->AsByteArray();
	std::copy(key, key + len, _key.begin());

	_initialized = true;
}

void AuthCrypt::DecryptRecv(uint8* data, size_t len)
{
	if (!_initialized) return;
	if (len < CRYPTED_RECV_LEN) return;

	for (size_t t = 0; t < CRYPTED_RECV_LEN; t++)
	{
		_recv_i %= _key.size();
		uint8 x = (data[t] - _recv_j) ^ _key[_recv_i];
		++_recv_i;
		_recv_j = data[t];
		data[t] = x;
	}
}

void AuthCrypt::EncryptSend(uint8* data, size_t len)
{
	if (!_initialized) return;
	if (len < CRYPTED_SEND_LEN) return;

	for (size_t t = 0; t < CRYPTED_SEND_LEN; t++)
	{
		_send_i %= _key.size();
		uint8 x = (data[t] ^ _key[_send_i]) + _send_j;
		++_send_i;
		data[t] = _send_j = x;
	}
}
