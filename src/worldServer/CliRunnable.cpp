#include "CliRunnable.h"
#include <Common.h>
#include <Log.h>
#include <World\World.h>
#include <Config/Config.h>
#include <Util/Util.h>
#include "md5.h"
#include <Database/DatabaseEnv.h>

void commandFinished(bool /*sucess*/)
{
	printf("origin>");
	fflush(stdout);
}

void SkipWhiteSpaces(char** args)
{
	if (!*args)
		return;
	while (isWhiteSpace(**args))
		++(*args);
}
bool normalizeString(std::string& utf8str)
{
	wchar_t wstr_buf[16 + 1];
	size_t wstr_len = 16;

	if (!Utf8toWStr(utf8str, wstr_buf, wstr_len))
		return false;

	for (uint32 i = 0; i <= wstr_len; ++i)
		wstr_buf[i] = wcharToUpperOnlyLatin(wstr_buf[i]);

	return WStrToUtf8(wstr_buf, wstr_len, utf8str);
}
bool HandleAccountCreateCommand(std::string& account_name, std::string& password)
{
	normalizeString(account_name);
	unsigned char digest[MD5_DIGEST_LENGTH];

	MD5((unsigned char*)password.c_str(), strlen(password.c_str()), (unsigned char*)&digest);

	char mdString[33];
	for (int i = 0; i < 16; i++)
		sprintf(&mdString[i * 2], "%02x", (unsigned int)digest[i]);
	if (!LoginDatabase.PExecute("INSERT INTO account(username,md5) VALUES('%s','%s')", account_name.c_str(), mdString))
		return false;                       // unexpected error
	LoginDatabase.Execute("INSERT INTO realmcharacters (realmid, acctid, numchars) SELECT realmlist.id, account.id, 0 FROM realmlist,account LEFT JOIN realmcharacters ON acctid=account.id WHERE acctid IS NULL");
                                       // everything's fine
	return true;
}
bool HandleCommande(char* args)
{
	std::istringstream buf(args);
	std::istream_iterator<std::string> beg(buf), end;

	std::vector<std::string> tokens(beg, end); // done!

	if (tokens.size() >= 4)
	{
		std::string command = tokens[0];
		std::string value = tokens[1];

		if (command == ".account")
		{
			if (value == "create")
			{
				if (HandleAccountCreateCommand(tokens[2], tokens[3]) == false)
				{
					sLog.outError("Account cannot be created, already exist?");
				}
				else
					sLog.outString("Account created");
			}
		}
	}
	commandFinished(true);
	return true;
}
void utf8print(const char* str)
{
	wchar_t wtemp_buf[6000];
	size_t wtemp_len = 6000 - 1;
	if (!Utf8toWStr(str, strlen(str), wtemp_buf, wtemp_len))
		return;

	char temp_buf[6000];
	CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
	printf("%s", temp_buf);
}
/// %Thread start
void CliRunnable::run()
{
	///- Init new SQL thread for the world database (one connection call enough)
	WorldDatabase.ThreadStart();                            // let thread do safe mySQL requests

	char commandbuf[256];

	///- Display the list of available CLI functions then beep
	sLog.outString();

	if (sConfig.GetBoolDefault("BeepAtStart", true))
		printf("\a");                                       // \a = Alert

															// print this here the first time
															// later it will be printed after command queue updates
	printf("origin>");

	///- As long as the World is running (no World::m_stopEvent), get the command line and handle it
	while (!World::IsStopped())
	{
		fflush(stdout);
#ifdef linux
		while (!kb_hit_return() && !World::IsStopped())
			// With this, we limit CLI to 10commands/second
			usleep(100);
		if (World::IsStopped())
			break;
#endif
		char* command_str = fgets(commandbuf, sizeof(commandbuf), stdin);
		if (command_str != nullptr)
		{
			for (int x = 0; command_str[x]; ++x)
				if (command_str[x] == '\r' || command_str[x] == '\n')
				{
					command_str[x] = 0;
					break;
				}


			if (!*command_str)
			{
				printf("origin>");
				continue;
			}

			std::string command;
			if (!consoleToUtf8(command_str, command))       // convert from console encoding to utf8
			{
				printf("origin>");
				continue;
			}

			//sWorld.QueueCliCommand(new CliCommandHolder(0, SEC_CONSOLE, command.c_str(), &utf8print, &commandFinished));
			HandleCommande(command_str);
		}
		else if (feof(stdin))
		{
			World::StopNow(SHUTDOWN_EXIT_CODE);
		}
	}

	///- End the database thread
	WorldDatabase.ThreadEnd();                              // free mySQL thread resources
}
