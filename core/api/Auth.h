// LINUX (new code): web interface account (user name + password), Bench 872.
//  - The account is in <profile>/emuled.users (permissions 0600): name, random salt, PBKDF2-HMAC-SHA256 hash of the password and a "secret" that signs sessions.
//    The password cannot be recovered, only reset (emuled --reset-user): it also changes the secret, so all open sessions expire.
//  - The session is a signed token (HMAC) "s1.<name in hex>|<expiry>.<signature>": sessions need not be remembered, it survives a daemon restart.
//  - The file is reread when it changes: --reset-user also works with the daemon running.
//  - The first account is created only with the "setup code" (the token of the emuled.token file): whoever reaches the port but does not have the code cannot take over the interface.
//  - After too many wrong attempts from the same address the login is blocked for a minute.
#pragma once
#include <ctime>
#include <map>
#include <mutex>
#include <string>

namespace emule::api {

class Auth
{
public:
	explicit Auth(std::string profile_dir);					// profile_dir with or without trailing "/"
	static bool ValidName(const std::string &name);			// 1-32 characters: letters, digits, . _ -
	static bool ValidPassword(const std::string &pw, std::string &err);	// at least 8 characters
	bool Configured();										// does an account exist?
	// Creates or replaces the account (new salt, new secret: previous sessions expire). false + err if invalid or not writable.
	bool SetUser(const std::string &name, const std::string &password, std::string &err);
	bool CheckLogin(const std::string &name, const std::string &password);	// constant-time comparison; the hash is always computed (no difference between wrong name and wrong password)
	std::string IssueToken(time_t now, long ttl_s = 30L * 24 * 3600);	// empty if there is no account
	bool VerifyToken(const std::string &token, time_t now);
	// limit on wrong attempts, per address
	bool AllowAttempt(const std::string &peer, time_t now);
	void RecordFailure(const std::string &peer, time_t now);
	void RecordSuccess(const std::string &peer);
	// First-start web Wizard: <profile>/emuled.setup is "pending" after the first account is created and "done" when the procedure ends; without the file (profiles
	// from before this feature) there is nothing to do.
	bool SetupPending();
	void MarkSetup(bool pending);
	void SetIterations(unsigned n) { iter_override_ = n; }	// tests only: fewer PBKDF2 rounds

private:
	struct User { std::string name, salt, hash, secret; unsigned iter = 0; };
	bool Load(User &u);										// rereads the file if it changed
	std::string path_, setup_path_;
	std::mutex m_;
	User user_; bool loaded_ = false; long long stamp_ = -1;
	unsigned iter_override_ = 0;
	struct Fails { int n = 0; time_t first = 0, lock_until = 0; };
	std::map<std::string, Fails> fails_;
};

} // namespace emule::api
