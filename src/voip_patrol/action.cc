/*
 * Copyright (C) 2016-2018 Julien Chavanton <jchavanton@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA~
 */

#include <unistd.h>
#include "voip_patrol.hh"
#include "action.hh"

Action::Action(Config *cfg) : config{cfg} {
	init_actions_params();
	std::cout<<"Prepared for Action!\n";
}

vector<ActionParam> Action::get_params(string name) {
	if (name.compare("call") == 0) return do_call_params;
	if (name.compare("register") == 0) return do_register_params;
	if (name.compare("wait") == 0) return do_wait_params;
	if (name.compare("accept") == 0) return do_accept_params;
	if (name.compare("alert") == 0) return do_alert_params;
	vector<ActionParam> empty_params;
	return empty_params;
}

string Action::get_env(string env) {
	if (const char* val = std::getenv(env.c_str())) {
		std::string s(val);
		return s;
	} else {
		return "";
	}
}

bool Action::set_param(ActionParam &param, const char *val) {
			if (!val) return false;
			if (param.type == APType::apt_bool) {
				param.b_val = true;
			} else if (param.type == APType::apt_integer) {
				param.i_val = atoi(val);
			} else if (param.type == APType::apt_float) {
				param.f_val = atof(val);
			} else {
				param.s_val = val;
				if (param.s_val.compare(0, 7, "VP_ENV_") == 0)
						param.s_val = get_env(val);
			}
			return true;
}

bool Action::set_param_by_name(vector<ActionParam> *params, const string name, const char *val) {
	for (auto &param : *params) {
		if (param.name.compare(name) == 0) {
				return set_param(param, val);
		}
	}
	return false;
}

void Action::init_actions_params() {
	// do_call
	do_call_params.push_back(ActionParam("caller", true, APType::apt_string));
	do_call_params.push_back(ActionParam("callee", true, APType::apt_string));
	do_call_params.push_back(ActionParam("label", false, APType::apt_string));
	do_call_params.push_back(ActionParam("username", false, APType::apt_string));
	do_call_params.push_back(ActionParam("password", false, APType::apt_string));
	do_call_params.push_back(ActionParam("realm", false, APType::apt_string));
	do_call_params.push_back(ActionParam("transport", false, APType::apt_string));
	do_call_params.push_back(ActionParam("expected_cause_code", false, APType::apt_integer));
	do_call_params.push_back(ActionParam("wait_until", false, APType::apt_string));
	do_call_params.push_back(ActionParam("max_duration", false, APType::apt_integer));
	do_call_params.push_back(ActionParam("min_mos", false, APType::apt_float));
	do_call_params.push_back(ActionParam("rtp_stats", false, APType::apt_bool));
	do_call_params.push_back(ActionParam("hangup", false, APType::apt_string));
	do_call_params.push_back(ActionParam("play", false, APType::apt_string));
	do_call_params.push_back(ActionParam("play_dtmf", false, APType::apt_string));
	do_call_params.push_back(ActionParam("repeat", false, APType::apt_integer));
	do_call_params.push_back(ActionParam("sps", false, APType::apt_float));
	// do_register
	do_register_params.push_back(ActionParam("transport", false, APType::apt_string));
	do_register_params.push_back(ActionParam("label", false, APType::apt_string));
	do_register_params.push_back(ActionParam("registrar", false, APType::apt_string));
	do_register_params.push_back(ActionParam("proxy", false, APType::apt_string));
	do_register_params.push_back(ActionParam("realm", false, APType::apt_string));
	do_register_params.push_back(ActionParam("username", false, APType::apt_string));
	do_register_params.push_back(ActionParam("account", false, APType::apt_string));
	do_register_params.push_back(ActionParam("password", false, APType::apt_string));
	do_register_params.push_back(ActionParam("expected_cause_code", false, APType::apt_integer));
	// do_accept
	do_accept_params.push_back(ActionParam("account", false, APType::apt_string));
	do_accept_params.push_back(ActionParam("transport", false, APType::apt_string));
	do_accept_params.push_back(ActionParam("label", false, APType::apt_string));
	do_accept_params.push_back(ActionParam("max_duration", false, APType::apt_integer));
	do_accept_params.push_back(ActionParam("ring_duration", false, APType::apt_integer));
	do_accept_params.push_back(ActionParam("wait_until", false, APType::apt_string));
	do_accept_params.push_back(ActionParam("hangup", false, APType::apt_integer));
	do_accept_params.push_back(ActionParam("min_mos", false, APType::apt_float));
	do_accept_params.push_back(ActionParam("rtp_stats", false, APType::apt_bool));
	do_accept_params.push_back(ActionParam("play", false, APType::apt_string));
	do_accept_params.push_back(ActionParam("code", false, APType::apt_integer));
	do_accept_params.push_back(ActionParam("expected_cause_code", false, APType::apt_integer));
	do_accept_params.push_back(ActionParam("reason", false, APType::apt_string));
	do_accept_params.push_back(ActionParam("play_dtmf", false, APType::apt_string));
	// do_wait
	do_wait_params.push_back(ActionParam("ms", false, APType::apt_integer));
	do_wait_params.push_back(ActionParam("complete", false, APType::apt_bool));
	// do_alert
	do_alert_params.push_back(ActionParam("email", false, APType::apt_string));
	do_alert_params.push_back(ActionParam("email_from", false, APType::apt_string));
	do_alert_params.push_back(ActionParam("smtp_host", false, APType::apt_string));
}

void Action::do_register(vector<ActionParam> &params) {
	string type {"register"};
	string transport {};
	string label {};
	string registrar {};
	string proxy {};
	string realm {};
	string username {};
	string account_name {};
	string password {};
	int expected_cause_code {200};

	for (auto param : params) {
		if (param.name.compare("transport") == 0) transport = param.s_val;
		else if (param.name.compare("label") == 0) label = param.s_val;
		else if (param.name.compare("registrar") == 0) registrar = param.s_val;
		else if (param.name.compare("proxy") == 0) proxy = param.s_val;
		else if (param.name.compare("realm") == 0) realm = param.s_val;
		else if (param.name.compare("account") == 0) account_name = param.s_val;
		else if (param.name.compare("username") == 0) username = param.s_val;
		else if (param.name.compare("password") == 0) password = param.s_val;
		else if (param.name.compare("expected_cause_code") == 0) expected_cause_code = param.i_val;
	}

	if (username.empty() || realm.empty() || password.empty() || registrar.empty()) {
		LOG(logERROR) <<__FUNCTION__<<" missing action parameter" ;
		return;
	}

	if (account_name.empty()) account_name = username;
	Test *test = new Test(config, type);
	test->local_user = username;
	test->remote_user = username;
	test->label = label;
	test->expected_cause_code = expected_cause_code;
	test->from = username;
	test->type = type;

	LOG(logINFO) <<__FUNCTION__<< " sip:" + account_name + "@" + registrar  ;
	AccountConfig acc_cfg;
	SipHeader sh;
	sh.hName = "User-Agent";
	sh.hValue = "<voip_patrol>";
	acc_cfg.regConfig.headers.push_back(sh);

	acc_cfg.sipConfig.transportId = config->transport_id_udp;
	if (!transport.empty()) {
		if (transport.compare("tcp") == 0) {
			acc_cfg.sipConfig.transportId = config->transport_id_tcp;
		} else if (transport.compare("tls") == 0) {
			if (config->transport_id_tls == -1) {
				LOG(logERROR) <<__FUNCTION__<<" TLS transport not supported";
				return;
			}
			acc_cfg.sipConfig.transportId = config->transport_id_tls;
		}
	}
	if (acc_cfg.sipConfig.transportId == config->transport_id_tls) {
		acc_cfg.idUri = "sips:" + account_name + "@" + registrar;
		acc_cfg.regConfig.registrarUri = "sips:" + registrar;
		if (!proxy.empty())
			acc_cfg.sipConfig.proxies.push_back("sips:" + proxy);

		LOG(logINFO) <<__FUNCTION__<< " SIPS URI Scheme";
	} else {
		LOG(logINFO) <<__FUNCTION__<< " SIP URI Scheme";
		acc_cfg.idUri = "sip:" + account_name + "@" + registrar;
		acc_cfg.regConfig.registrarUri = "sip:" + registrar;
		if (!proxy.empty())
			acc_cfg.sipConfig.proxies.push_back("sip:" + proxy);

	}
	acc_cfg.sipConfig.authCreds.push_back( AuthCredInfo("digest", realm, username, 0, password) );

	TestAccount *acc = config->findAccount(account_name);
	if (!acc) {
		acc = config->createAccount(acc_cfg);
	} else {
		acc->modify(acc_cfg);
	}
	acc->setTest(test);
}

void Action::do_accept(vector<ActionParam> &params) {
	string type {"accept"};
	string account_name {};
	string transport {};
	string label {};
	string play {default_playback_file};
	string play_dtmf {};
	float min_mos {0.0};
	int max_duration {0};
	int ring_duration {0};
	int hangup_duration {0};
	call_state_t wait_until {INV_STATE_NULL};
	bool rtp_stats {false};
	int code {200};
	int expected_cause_code {200};
	string reason {};

	for (auto param : params) {
		if (param.name.compare("account") == 0) account_name = param.s_val;
		else if (param.name.compare("transport") == 0) transport = param.s_val;
		else if (param.name.compare("play") == 0 && param.s_val.length() > 0) play = param.s_val;
		else if (param.name.compare("play_dtmf") == 0 && param.s_val.length() > 0) play_dtmf = param.s_val;
		else if (param.name.compare("code") == 0) code = param.i_val;
		else if (param.name.compare("expected_cause_code") == 0) expected_cause_code = param.i_val;
		else if (param.name.compare("reason") == 0 && param.s_val.length() > 0) reason = param.s_val;
		else if (param.name.compare("label") == 0) label = param.s_val;
		else if (param.name.compare("max_duration") == 0) max_duration = param.i_val;
		else if (param.name.compare("ring_duration") == 0) ring_duration = param.i_val;
		else if (param.name.compare("min_mos") == 0) min_mos = param.f_val;
		else if (param.name.compare("rtp_stats") == 0) rtp_stats = param.b_val;
		else if (param.name.compare("wait_until") == 0) wait_until = get_call_state_from_string(param.s_val);
		else if (param.name.compare("hangup") == 0) hangup_duration = param.i_val;
	}

	if (account_name.empty()) {
		LOG(logERROR) <<__FUNCTION__<<" missing action parameters <account>" ;
		return;
	}

	TestAccount *acc = config->findAccount(account_name);
	if (!acc) {
		AccountConfig acc_cfg;
		acc_cfg.sipConfig.transportId = config->transport_id_udp;
		if (!transport.empty()) {
			if (transport.compare("tcp") == 0) {
				acc_cfg.sipConfig.transportId = config->transport_id_tcp;
			} else if (transport.compare("tls") == 0) {
				if (config->transport_id_tls == -1) {
					LOG(logERROR) <<__FUNCTION__<<": TLS transport not supported.";
					return;
				}
				acc_cfg.sipConfig.transportId = config->transport_id_tls;
			}
		}
		if (acc_cfg.sipConfig.transportId == config->transport_id_tls) {
			acc_cfg.idUri = "sips:" + account_name;
		} else {
			acc_cfg.idUri = "sip:" + account_name;
		}
		acc = config->createAccount(acc_cfg);
	}
	acc->hangup_duration = hangup_duration;
	acc->max_duration = max_duration;
	acc->ring_duration = ring_duration;
	acc->accept_label = label;
	acc->rtp_stats = rtp_stats;
	acc->play = play;
	acc->play_dtmf = play_dtmf;
	acc->wait_state = wait_until;
	acc->reason = reason;
	acc->code = code;
	acc->expected_cause_code = expected_cause_code;
}

vector<string> Action::parse_args(istringstream is) {
	vector<string> args;
	string token;
	while (getline(is, token, ',')) {
	    if (!token.empty())
		    args.push_back(token);
	}
	return args;
}

void Action::do_call(vector<ActionParam> &params, SipHeaderVector &x_headers) {
	string type {"call"};
	string play {default_playback_file};
	string play_dtmf {};
	string caller {};
	string callee {};
	string transport {};
	string username {};
	string password {};
	string realm {};
	string label {};
	int expected_cause_code {200};
	call_state_t wait_until {INV_STATE_NULL};
	float min_mos {0.0};
	int max_duration {0};
	int max_calling_duration {0};
	int expected_duration {0};
	istringstream hangup_duration_s;
	vector<string> hangup_duration_args;
	int hangup_duration {0};
	int repeat {0};
	float sps {1.0};
	int interval {1000};
	bool recording {false};
	bool rtp_stats {false};

	for (auto param : params) {
		if (param.name.compare("callee") == 0) callee = param.s_val;
		else if (param.name.compare("caller") == 0) caller = param.s_val;
		else if (param.name.compare("transport") == 0) transport = param.s_val;
		else if (param.name.compare("play") == 0 && param.s_val.length() > 0) play = param.s_val;
		else if (param.name.compare("play_dtmf") == 0 && param.s_val.length() > 0) play_dtmf = param.s_val;
		else if (param.name.compare("username") == 0) username = param.s_val;
		else if (param.name.compare("password") == 0) password = param.s_val;
		else if (param.name.compare("realm") == 0) realm = param.s_val;
		else if (param.name.compare("label") == 0) label = param.s_val;
		else if (param.name.compare("expected_cause_code") == 0) expected_cause_code = param.i_val;
		else if (param.name.compare("wait_until") == 0) wait_until = get_call_state_from_string(param.s_val);
		else if (param.name.compare("min_mos") == 0) min_mos = param.f_val;
		else if (param.name.compare("rtp_stats") == 0) rtp_stats = param.b_val;
		else if (param.name.compare("max_duration") == 0) max_duration = param.i_val;
		else if (param.name.compare("max_calling_duration") == 0) max_calling_duration = param.i_val;
		else if (param.name.compare("duration") == 0) expected_duration = param.i_val;
		else if (param.name.compare("hangup") == 0) hangup_duration_s.str(param.s_val);
		else if (param.name.compare("repeat") == 0) repeat = param.i_val;
		else if (param.name.compare("sps") == 0) sps = param.f_val;
		else if (param.name.compare("recording") == 0) recording = true;
	}

	hangup_duration_args = parse_args(hangup_duration_s);
	if (!hangup_duration_args.empty()) {
		LOG(logINFO) << __FUNCTION__ << hangup_duration_args[0];
	}

	if (repeat) {
		interval = 1000/sps;
	}

	if (caller.empty() || callee.empty()) {
		LOG(logERROR) <<__FUNCTION__<<": missing action parameters for callee/caller" ;
		return;
	}

	TestAccount* acc = config->findAccount(caller);
	if (!acc) {
		AccountConfig acc_cfg;
		acc_cfg.sipConfig.transportId = config->transport_id_udp;
		if (!transport.empty()) {
			if (transport.compare("tcp") == 0) {
				acc_cfg.sipConfig.transportId = config->transport_id_tcp;
			} else if (transport.compare("tls") == 0) {
				if (config->transport_id_tls == -1) {
					LOG(logERROR) <<__FUNCTION__<<": TLS transport not supported" ;
					return;
				}
				acc_cfg.sipConfig.transportId = config->transport_id_tls;
			}
		}
		if (acc_cfg.sipConfig.transportId == config->transport_id_tls) {
			acc_cfg.idUri = "sips:" + caller;
		} else {
			acc_cfg.idUri = "sip:" + caller;
		}
		if (!realm.empty()) {
			if (username.empty() || password.empty()) {
				if (username.empty()) LOG(logERROR) <<__FUNCTION__<< ": realm specified missing username";
				else LOG(logERROR) <<__FUNCTION__<<": realm specified missing password";
				return;
			}
			acc_cfg.sipConfig.authCreds.push_back( AuthCredInfo("digest", realm, username, 0, password) );
		}
		acc = config->createAccount(acc_cfg);
	}

	do {
		Test *test = new Test(config, type);
		test->wait_state = wait_until;
		if (test->wait_state != INV_STATE_NULL)
			test->state = VPT_RUN_WAIT;
		test->expected_duration = expected_duration;
		test->label = label;
		test->play = play;
		test->play_dtmf = play_dtmf;
		test->min_mos = min_mos;
		test->max_duration = max_duration;
		test->max_calling_duration = max_calling_duration;
		test->hangup_duration = hangup_duration;
		test->recording = recording;
		test->rtp_stats = rtp_stats;
		std::size_t pos = caller.find("@");
		if (pos!=std::string::npos) {
			test->local_user = caller.substr(0, pos);
		}
		pos = callee.find("@");
		if (pos!=std::string::npos) {
			test->remote_user = callee.substr(0, pos);
		}

		TestCall *call = new TestCall(acc);
		config->calls.push_back(call);

		call->test = test;
		test->expected_cause_code = expected_cause_code;
		test->from = caller;
		test->to = callee;
		test->type = type;
		acc->calls.push_back(call);
		CallOpParam prm(true);

		for (auto x_hdr : x_headers) {
			prm.txOption.headers.push_back(x_hdr);
		}

		prm.opt.audioCount = 1;
		prm.opt.videoCount = 0;
		LOG(logINFO) << "call->test:" << test << " " << call->test->type;
		LOG(logINFO) << "calling :" +callee;
		if (transport.compare("tls") == 0) {
			try {
				call->makeCall("sips:"+callee, prm);
			} catch (pj::Error e)  {
				LOG(logERROR) <<__FUNCTION__<<" error :" << e.status << std::endl;
			}
		} else if (transport.compare("tcp") == 0) {
			try {
				call->makeCall("sip:"+callee+";transport=tcp", prm);
			} catch (pj::Error e)  {
				LOG(logERROR) <<__FUNCTION__<<" error :" << e.status << std::endl;
			}
		} else {
			try {
				call->makeCall("sip:"+callee, prm);
			} catch (pj::Error e)  {
				LOG(logERROR) <<__FUNCTION__<<" error :" << e.status << std::endl;
			}
		}
		repeat--;
		pj_thread_sleep(interval);
	} while (repeat >= 0);
}

void Action::do_alert(vector<ActionParam> &params) {
	string email {};
	string email_from {};
	string smtp_host {};
	for (auto param : params) {
		if (param.name.compare("email") == 0) email = param.s_val;
		else if (param.name.compare("email_from") == 0) email_from = param.s_val;
		else if (param.name.compare("smtp_host") == 0) smtp_host = param.s_val;
	}
	LOG(logINFO) << __FUNCTION__ << "email to:"<<email<< " from:"<<email_from;
	config->alert_email_to = email;
	config->alert_email_from = email_from;
	config->alert_server_url = smtp_host;
}

void Action::do_wait(vector<ActionParam> &params) {
	int duration_ms = 0;
	bool complete_all = false;
	for (auto param : params) {
		if (param.name.compare("ms") == 0) duration_ms = param.i_val;
		if (param.name.compare("complete") == 0) complete_all = param.b_val;
	}
	LOG(logINFO) << __FUNCTION__ << " duration_ms:" << duration_ms << " complete all tests:" << complete_all;
	bool completed = false;
	int tests_running = 0;
	bool status_update = true;
	while (!completed) {
		for (auto & account : config->accounts) {
			AccountInfo acc_inf = account->getInfo();
			if (account->test && account->test->state == VPT_DONE){
				delete account->test;
				account->test = NULL;
			} else if (account->test) {
				tests_running++;
			}
		}
		for (auto & call : config->calls) {
			if (call->test && call->test->state == VPT_DONE){
				//LOG(logINFO) << "delete call test["<<call->test<<"]";
				//delete call->test;
				//call->test = NULL;
				//removeCall(call);
			} else if (call->test) {
				CallInfo ci = call->getInfo();
				if (status_update) {
					LOG(logDEBUG) <<__FUNCTION__<<": [call]["<<call->getId()<<"][test]["<<(ci.role==0?"CALLER":"CALLEE")<<"]["
						     << ci.callIdString <<"]["<<ci.remoteUri<<"]["<<ci.stateText<<"|"<<ci.state<<"]duration["
						     << ci.connectDuration.sec <<">="<<call->test->hangup_duration<<"]";
				}
				if (ci.state == PJSIP_INV_STATE_CALLING || ci.state == PJSIP_INV_STATE_EARLY)  {
					Test *test = call->test;
					if (test->ring_duration > 0 && ci.totalDuration.sec >= test->ring_duration) {
						CallOpParam prm;
						if (test->reason.size() > 0) prm.reason = test->reason;
						if (test->code) prm.statusCode = test->code;
						call->answer(prm);
					} else if (test->max_calling_duration && test->max_calling_duration <= ci.totalDuration.sec) {
						LOG(logINFO) <<__FUNCTION__<<"[cancelling:call]["<<call->getId()<<"][test]["<<(ci.role==0?"CALLER":"CALLEE")<<"]["
						     << ci.callIdString <<"]["<<ci.remoteUri<<"]["<<ci.stateText<<"|"<<ci.state<<"]duration["
						     << ci.totalDuration.sec <<">="<<test->max_calling_duration<<"]";
						CallOpParam prm(true);
						try {
							call->hangup(prm);
						} catch (pj::Error e)  {
							if (e.status != 171140) LOG(logERROR) <<__FUNCTION__<<" error :" << e.status;
						}
					}
				} else if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
					std::string res = "call[" + std::to_string(ci.lastStatusCode) + "] reason["+ ci.lastReason +"]";
					call->test->connect_duration = ci.connectDuration.sec;
					call->test->setup_duration = ci.totalDuration.sec - ci.connectDuration.sec;
					call->test->result_cause_code = (int)ci.lastStatusCode;
					call->test->reason = ci.lastReason;
					if (call->test->hangup_duration && ci.connectDuration.sec >= call->test->hangup_duration){
						if (ci.state == PJSIP_INV_STATE_CONFIRMED) {
							CallOpParam prm(true);
							LOG(logINFO) << "hangup : call in PJSIP_INV_STATE_CONFIRMED" ;
							try {
								call->hangup(prm);
							} catch (pj::Error e)  {
								if (e.status != 171140) LOG(logERROR) <<__FUNCTION__<<" error :" << e.status << std::endl;
							}
						}
						call->test->update_result();
					}
				}
				if (complete_all || call->test->state == VPT_RUN_WAIT)
					tests_running++;
			}
		}

		int pos=0;
		for (auto test : config->tests_with_rtp_stats) {
			if (test->rtp_stats_ready) {
				test->update_result();
				config->tests_with_rtp_stats.erase(config->tests_with_rtp_stats.begin()+pos);
				LOG(logINFO) << __FUNCTION__ << " erase pos:" << pos;
				pos++;
			}
		}

		if (tests_running > 0) {
			if (status_update) {
				LOG(logINFO) <<__FUNCTION__<<LOG_COLOR_ERROR<<": action[wait] active account tests or call tests in run_wait["<<tests_running<<"] <<<<"<<LOG_COLOR_END;
				status_update = false;
			}
			tests_running=0;

			if (duration_ms > 0) duration_ms -= 100;
			pj_thread_sleep(100);
		} else {
			if (duration_ms > 0) {
				duration_ms -= 10;
				pj_thread_sleep(10);
				continue;
			} else if (duration_ms == -1) {
				pj_thread_sleep(10);
				continue;
			}

			completed = true;
			LOG(logINFO) <<__FUNCTION__<<": completed";
		}
	}
}
