#ifndef _AUTHCRYPT_H
#define _AUTHCRYPT_H

#include "../Common.h"
#include <vector>

class BigNumber;

class AuthCrypt
{
public:
	AuthCrypt();

	void Init(BigNumber* K);

	void DecryptRecv(uint8*, size_t);
	void EncryptSend(uint8*, size_t);

	bool IsInitialized() const { return _initialized; }

private:
	const static size_t CRYPTED_SEND_LEN = 4;
	const static size_t CRYPTED_RECV_LEN = 6;

	std::vector<uint8> _key;
	uint8 _send_i, _send_j, _recv_i, _recv_j;
	bool _initialized;
};
#endif