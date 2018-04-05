/*
 *   Copyright (C) 2018 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <exception>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <csignal>
#include <ctime>

#include "versions.h"
#include "mmdvm_modem.h"

std::atomic<bool> CMMDVMModem::keep_running(true);

CMMDVMModem::CMMDVMModem()
{
}

CMMDVMModem::~CMMDVMModem()
{
}

bool CMMDVMModem::Initialize(const char *cfgfile)
{
	if (ReadConfig(cfgfile))
		return true;

	struct sigaction act;
	act.sa_handler = &CMMDVMModem::SignalCatch;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGTERM, &act, 0) != 0) {
		printf("sigaction-TERM failed, error=%d\n", errno);
		return true;
	}
	if (sigaction(SIGHUP, &act, 0) != 0) {
		printf("sigaction-HUP failed, error=%d\n", errno);
		return true;
	}
	if (sigaction(SIGINT, &act, 0) != 0) {
		printf("sigaction-INT failed, error=%d\n", errno);
		return true;
	}

//	dstar_dv_init();

	try {
		mmdvm_future = std::async(std::launch::async, &CMMDVMModem::ProcessMMDVM, this);
	} catch (const std::exception &e) {
		printf("Unable to start ReadDVAPThread(). Exception: %s\n", e.what());
		keep_running = false;
	}
	printf("Started ProcessMMDVM() thread\n");

	return false;
}

void CMMDVMModem::ProcessGateway()
{
}

void CMMDVMModem::ProcessMMDVM()
{
}

bool CMMDVMModem::GetValue(const Config &cfg, const char *path, int &value, const int min, const int max, const int default_value)
{
	if (cfg.lookupValue(path, value)) {
		if (value < min || value > max)
			value = default_value;
	} else
		value = default_value;
	printf("%s = [%d]\n", path, value);
	return true;
}

bool CMMDVMModem::GetValue(const Config &cfg, const char *path, double &value, const double min, const double max, const double default_value)
{
	if (cfg.lookupValue(path, value)) {
		if (value < min || value > max)
			value = default_value;
	} else
		value = default_value;
	printf("%s = [%lg]\n", path, value);
	return true;
}

bool CMMDVMModem::GetValue(const Config &cfg, const char *path, bool &value, const bool default_value)
{
	if (! cfg.lookupValue(path, value))
		value = default_value;
	printf("%s = [%s]\n", path, value ? "true" : "false");
	return true;
}

bool CMMDVMModem::GetValue(const Config &cfg, const char *path, std::string &value, int min, int max, const char *default_value)
{
	if (cfg.lookupValue(path, value)) {
		int l = value.length();
		if (l<min || l>max) {
			printf("%s value '%s' is wrong size\n", path, value.c_str());
			return false;
		}
	} else
		value = default_value;
	printf("%s = [%s]\n", path, value.c_str());
	return true;
}

// process configuration file and return true if there was a problem
bool CMMDVMModem::ReadConfig(const char *cfgFile)
{
	Config cfg;

	printf("Reading file %s\n", cfgFile);
	// Read the file. If there is an error, report it and exit.
	try {
		cfg.readFile(cfgFile);
	}
	catch(const FileIOException &fioex) {
		printf("Can't read %s\n", cfgFile);
		return true;
	}
	catch(const ParseException &pex) {
		printf("Parse error at %s:%d - %s\n", pex.getFile(), pex.getLine(), pex.getError());
		return true;
	}

	std::string mmdvm_path, value;
	int i;
	for (i=0; i<3; i++) {
		mmdvm_path = "module.";
		mmdvm_path += ('a' + i);
		if (cfg.lookupValue(mmdvm_path + ".type", value)) {
			if (0 == strcasecmp(value.c_str(), "mmdvm"))
				break;
		}
	}
	if (i >= 3) {
		printf("mmdvm not defined in any module!\n");
		return true;
	}
	RPTR_MOD = 'A' + i;

	if (cfg.lookupValue(std::string(mmdvm_path+".callsign").c_str(), value) || cfg.lookupValue("ircddb.login", value)) {
		int l = value.length();
		if (l<3 || l>CALL_SIZE-2) {
			printf("Call '%s' is invalid length!\n", value.c_str());
			return true;
		} else {
			for (i=0; i<l; i++) {
				if (islower(value[i]))
					value[i] = toupper(value[i]);
			}
			value.resize(CALL_SIZE, ' ');
		}
		strcpy(RPTR, value.c_str());
	} else {
		printf("%s.login is not defined!\n", mmdvm_path.c_str());
		return true;
	}

	if (cfg.lookupValue("ircddb.login", value)) {
		int l = value.length();
		if (l<3 || l>CALL_SIZE-2) {
			printf("Call '%s' is invalid length!\n", value.c_str());
			return true;
		} else {
			for (i=0; i<l; i++) {
				if (islower(value[i]))
					value[i] = toupper(value[i]);
			}
			value.resize(CALL_SIZE, ' ');
		}
		strcpy(OWNER, value.c_str());
		printf("ircddb.login = [%s]\n", OWNER);
	} else {
		printf("ircddb.login is not defined!\n");
		return true;
	}

	if (GetValue(cfg, std::string(mmdvm_path+".internal_ip").c_str(), value, 7, IP_SIZE, "0.0.0.0"))
		strcpy(RPTR_VIRTUAL_IP, value.c_str());
	else
		return true;

	GetValue(cfg, std::string(mmdvm_path+".port").c_str(), i, 10000, 65535, 20010);
	RPTR_PORT = (unsigned short)i;

	if (GetValue(cfg, "gateway.ip", value, 7, IP_SIZE, "127.0.0.1"))
		strcpy(G2_INTERNAL_IP, value.c_str());
	else
		return true;

	GetValue(cfg, "gateway.internal.port", i, 10000, 65535, 20010);
	G2_PORT = (unsigned short)i;

	GetValue(cfg, "timing.play.delay", DELAY_BETWEEN, 9, 25, 19);

	GetValue(cfg, "timing.play.wait", DELAY_BEFORE, 1, 10, 2);

	GetValue(cfg, std::string(mmdvm_path+".acknowledge").c_str(), RPTR_ACK, false);

	GetValue(cfg, std::string(mmdvm_path+".packet_wait").c_str(), WAIT_FOR_PACKETS, 6, 100, 25);

	return false;
}

void CMMDVMModem::SignalCatch(int signum)
{
	if ((signum == SIGTERM) || (signum == SIGINT)  || (signum == SIGHUP))
		keep_running = false;
	exit(0);
}

int main(int argc, const char **argv)
{
	setbuf(stdout, NULL);
	if (2 != argc) {
		printf("usage: %s path_to_config_file\n", argv[0]);
		printf("       %s --version\n", argv[0]);
		return 1;
	}

	if ('-' == argv[1][0]) {
		printf("\nMMDVM Modem Version #%s Copyright (C) 2018 by Thomas A. Early N7TAE\n", MMDVM_VERSION);
		printf("MMDVM Modem comes with ABSOLUTELY NO WARRANTY; see the LICENSE for details.\n");
		printf("This is free software, and you are welcome to distribute it\nunder certain conditions that are discussed in the LICENSE file.\n\n");
		return 0;
	}

	CMMDVMModem mmdvm;

	if (mmdvm.Initialize(argv[1])) {
		printf("ERROR: Failed to process %s\n", argv[0]);
		return 1;
	}

	while (mmdvm.keep_running) {
		mmdvm.ProcessGateway();
	}

	mmdvm.mmdvm_future.get();
	printf("%s is closed.\n", argv[0]);

	return 0;
}
