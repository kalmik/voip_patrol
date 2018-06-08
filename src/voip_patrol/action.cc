/*
 * Voip Patrol
 * @author Julien Chavanton 2018
 */


#include "voip_patrol.hh"

Action::Action(Config *cfg) : config{cfg} {
	init_actions_params();
	std::cout<<"Prepared for Action!\n";
}

vector<ActionParam>* Action::get_params(string name) {
	if (name.compare("call") == 0) return &do_call_params;
	if (name.compare("wait") == 0) return &do_wait_params;
	return nullptr;
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
			if (param.type == APType::integer) {
				param.i_val = atoi(val);
			} else {
				param.s_val = val;
				if (param.s_val.compare(0, 7, "VP_ENV_") == 0)
						param.s_val = get_env(val);
			}
			return true;
}

void Action::init_actions_params() {
	// do_call
	do_call_params.push_back(ActionParam("caller", true, APType::string));
	do_call_params.push_back(ActionParam("callee", true, APType::string));
	do_call_params.push_back(ActionParam("label", false, APType::string));
	do_call_params.push_back(ActionParam("username", false, APType::string));
	do_call_params.push_back(ActionParam("password", false, APType::string));
	do_call_params.push_back(ActionParam("realm", false, APType::string));
	do_call_params.push_back(ActionParam("transport", false, APType::string));
	do_call_params.push_back(ActionParam("expected_cause_code", false, APType::integer));
	do_call_params.push_back(ActionParam("wait_until", false, APType::integer));
	do_call_params.push_back(ActionParam("max_duration", false, APType::integer));
	do_call_params.push_back(ActionParam("hangup", false, APType::integer));
	// do_wait
	do_wait_params.push_back(ActionParam("ms", false, APType::integer));
	do_wait_params.push_back(ActionParam("complete", false, APType::integer));
}

void Action::do_wait(vector<ActionParam> &params) {
	int duration_ms = 0;
	int complete_all = 0;
	for (auto param : params) {
		if (param.name.compare("ms") == 0) duration_ms = param.i_val;
		if (param.name.compare("complete") == 0) complete_all = param.i_val;
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
					LOG(logINFO) <<"[wait:call]["<<call->getId()<<"][test]["<<(ci.role==0?"CALLER":"CALLEE")<<"]["
				  		     << ci.callIdString <<"]["<<ci.remoteUri<<"]["<<ci.stateText<<"|"<<ci.state<<"]duration["
						     << ci.connectDuration.sec <<">="<<call->test->hangup_duration<<"]";
				}
				if (ci.state == PJSIP_INV_STATE_CALLING || ci.state == PJSIP_INV_STATE_EARLY)  {
					if (call->test->max_calling_duration && call->test->max_calling_duration <= ci.totalDuration.sec) {
						LOG(logINFO) <<"[cancelling:call]["<<call->getId()<<"][test]["<<(ci.role==0?"CALLER":"CALLEE")<<"]["
				  		     << ci.callIdString <<"]["<<ci.remoteUri<<"]["<<ci.stateText<<"|"<<ci.state<<"]duration["
						     << ci.totalDuration.sec <<">="<<call->test->max_calling_duration<<"]";
						CallOpParam prm(true);
						call->hangup(prm);
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
							call->hangup(prm);
						}
						if (call->role == 0 && call->test->min_mos > 0) {
							call->test->get_mos();
						}
						call->test->update_result();
					}
				}
				if (complete_all || call->test->state == VPT_RUN_WAIT)
					tests_running++;
			}
		}
		if (tests_running > 0) {
			if (status_update) {
				LOG(logINFO) <<LOG_COLOR_ERROR<<">>>> action[wait] active tests in run_wait["<<tests_running<<"] <<<<"<<LOG_COLOR_END;
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
			LOG(logINFO) << "action[wait] completed";
			config->update_result(std::string("fds")+"action[wait] completed");
		}
	}
}
