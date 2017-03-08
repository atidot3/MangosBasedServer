#ifndef _AUTHCODES_H
#define _AUTHCODES_H

enum eAuthCmd
{
	CMD_AUTH_LOGIN = 0x00,
	CMD_REALM_LIST = 0x01
};

enum AuthResult
{
	ORIGIN_SUCCESS = 0x00,
	ORIGIN_FAIL_UNKNOWN0 = 0x01,                 ///< ? Unable to connect
	ORIGIN_FAIL_UNKNOWN1 = 0x02,                 ///< ? Unable to connect
	ORIGIN_FAIL_BANNED = 0x03,                 ///< This <game> account has been closed and is no longer available for use. Please go to <site>/banned.html for further information.
	ORIGIN_FAIL_UNKNOWN_ACCOUNT = 0x04,                 ///< The information you have entered is not valid. Please check the spelling of the account name and password. If you need help in retrieving a lost or stolen password, see <site> for more information
	ORIGIN_FAIL_INCORRECT_PASSWORD = 0x05,                 ///< The information you have entered is not valid. Please check the spelling of the account name and password. If you need help in retrieving a lost or stolen password, see <site> for more information
	// client reject next login attempts after this error, so in code used ORIGIN_FAIL_UNKNOWN_ACCOUNT for both cases
	ORIGIN_FAIL_ALREADY_ONLINE = 0x06,                 ///< This account is already logged into <game>. Please check the spelling and try again.
	ORIGIN_FAIL_NO_TIME = 0x07,                 ///< You have used up your prepaid time for this account. Please purchase more to continue playing
	ORIGIN_FAIL_DB_BUSY = 0x08,                 ///< Could not log in to <game> at this time. Please try again later.
	ORIGIN_FAIL_VERSION_INVALID = 0x09,                 ///< Unable to validate game version. This may be caused by file corruption or interference of another program. Please visit <site> for more information and possible solutions to this issue.
	ORIGIN_FAIL_VERSION_UPDATE = 0x0A,                 ///< Downloading
	ORIGIN_FAIL_INVALID_SERVER = 0x0B,                 ///< Unable to connect
	ORIGIN_FAIL_SUSPENDED = 0x0C,                 ///< This <game> account has been temporarily suspended. Please go to <site>/banned.html for further information
	ORIGIN_FAIL_FAIL_NOACCESS = 0x0D,                 ///< Unable to connect
	ORIGIN_SUCCESS_SURVEY = 0x0E,                 ///< Connected.
	ORIGIN_FAIL_PARENTCONTROL = 0x0F,                 ///< Access to this account has been blocked by parental controls. Your settings may be changed in your account preferences at <site>
	ORIGIN_FAIL_LOCKED_ENFORCED = 0x10,                 ///< You have applied a lock to your account. You can change your locked status by calling your account lock phone number.
	ORIGIN_FAIL_TRIAL_ENDED = 0x11,                 ///< Your trial subscription has expired. Please visit <site> to upgrade your account.
	ORIGIN_FAIL_USE_ORIGINENET = 0x12,                 ///< ORIGIN_FAIL_OTHER This account is now attached to a Battle.net account. Please login with your Origin.net account email address and password.
												   // ORIGIN_FAIL_OVERMIND_CONVERTED
												   // ORIGIN_FAIL_ANTI_INDULGENCE
												   // ORIGIN_FAIL_EXPIRED
												   // ORIGIN_FAIL_NO_GAME_ACCOUNT
												   // ORIGIN_FAIL_BILLING_LOCK
												   // ORIGIN_FAIL_IGR_WITHOUT_BNET
												   // ORIGIN_FAIL_AA_LOCK
												   // ORIGIN_FAIL_UNLOCKABLE_LOCK
												   // ORIGIN_FAIL_MUST_USE_BNET
												   // ORIGIN_FAIL_OTHER
};

#endif
