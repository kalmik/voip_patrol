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

#include "voip_patrol.hh"
#include "action.hh"
#define THIS_FILE "voip_patrol.cpp"

using namespace pj;


void get_time_string(char * str_now) {
	time_t t = time(0);   // get time now
	struct tm * now = localtime( & t );
	sprintf(str_now,"%02d-%02d-%04d %02d:%02d:%02d", now->tm_mday, now->tm_mon+1, now->tm_year+1900, now->tm_hour, now->tm_min, now->tm_sec);
}

call_state_t get_call_state_from_string (string state) {
	if (state.compare("CALLING") == 0) return INV_STATE_CALLING;
	if (state.compare("INCOMING") == 0) return INV_STATE_INCOMING;
	if (state.compare("EARLY") == 0) return INV_STATE_EARLY;
	if (state.compare("CONNECTING") == 0) return INV_STATE_CONNECTING;
	if (state.compare("CONFIRMED") == 0) return INV_STATE_CONFIRMED;
	if (state.compare("DISCONNECTED") == 0) return INV_STATE_DISCONNECTED;
	return INV_STATE_NULL;
}

string get_call_state_string (call_state_t state) {
	if (state == INV_STATE_CALLING) return "CALLING";
	if (state == INV_STATE_INCOMING) return "INCOMING";
	if (state == INV_STATE_EARLY) return "EARLY";
	if (state == INV_STATE_CONNECTING) return "CONNECTING";
	if (state == INV_STATE_CONFIRMED) return "CONFIRMED";
	if (state == INV_STATE_DISCONNECTED) return "DISCONNECTED";
	return "NULL";
}

/*
 * TestCall implementation
 */

TestCall::TestCall(TestAccount *p_acc, int call_id) : Call(*p_acc, call_id) {
	test = NULL;
	acc = p_acc;
	recorder_id = -1;
	player_id = -1;
	role = -1; // Caller 0 | callee 1
}

TestCall::~TestCall() {
	if (test) {
		LOG(logINFO) << "delete call test["<<test<<"]";
		delete test;
	}
}

void TestCall::setTest(Test *p_test) {
	test = p_test;
}


void TestCall::onCallRxOffer(OnCallTsxStateParam &prm) {
	PJ_UNUSED_ARG(prm);
	CallInfo ci = getInfo();
	LOG(logDEBUG) <<__FUNCTION__<<": ["<<getId()<<"]["<<ci.remoteUri<<"]["<<ci.stateText<<"]id["<<ci.callIdString<<"]";
}

void TestCall::onCallTsxState(OnCallTsxStateParam &prm) {
	PJ_UNUSED_ARG(prm);
	CallInfo ci = getInfo();
	LOG(logINFO) <<__FUNCTION__<<": ["<<getId()<<"]["<<ci.remoteUri<<"]["<<ci.stateText<<"]id["<<ci.callIdString<<"]";
	// if (ci.stateText.compare("INCOMING")  == 0 ) pj_thread_sleep(10000);
}

/* Convenient function to convert transmission factor to MOS */
static float rfactor_to_mos(float rfactor) {
	float mos;
	if (rfactor <= 0) {
		mos = 0.0;
	} else if (rfactor > 100) {
		mos = 4.5;
	} else {
		mos = rfactor*4.5/100;
	}
	return mos;
}

void TestCall::onDtmfDigit(OnDtmfDigitParam &prm) {
	LOG(logINFO) << __FUNCTION__ << ":"<<prm.digit;
	test->dtmf_recv.append(prm.digit);
}

void TestCall::onStreamDestroyed(OnStreamDestroyedParam &prm) {
	LOG(logDEBUG) <<__FUNCTION__<<": idx["<<prm.streamIdx<<"]";
	pjmedia_stream const *pj_stream = (pjmedia_stream *)&prm.stream;
	pjmedia_stream_info *pj_stream_info;
	try {
		StreamStat const &stats = getStreamStat(prm.streamIdx);
		RtcpStat rtcp = stats.rtcp;
		RtcpStreamStat rxStat = rtcp.rxStat;
		RtcpStreamStat txStat = rtcp.txStat;

		LOG(logINFO) << __FUNCTION__ << ": RTCP Rx jitter:"<<rxStat.jitterUsec.n<<"|"<<rxStat.jitterUsec.mean/1000<<"|"<<rxStat.jitterUsec.max/1000
                     <<"Usec pkt:"<<rxStat.pkt<<" Kbytes:"<<rxStat.bytes/1024<<" loss:"<<rxStat.loss<<" discard:"<<rxStat.discard;
		LOG(logINFO) << __FUNCTION__ << ": RTCP Tx jitter:"<<txStat.jitterUsec.n<<"|"<<txStat.jitterUsec.mean/1000<<"|"<<txStat.jitterUsec.max/1000
                     <<"Usec pkt:"<<txStat.pkt<<" Kbytes:"<<rxStat.bytes/1024<<" loss:"<< txStat.loss<<" discard:"<<txStat.discard;
		/* represent loss dependent effective equipment impairment factor and percentage loss probability */
		const int Bpl = 25; /* packet-loss robustness factor Bpl is defined as a codec-specific value. */
		float Ie_eff_rx, Ppl_rx, Ppl_cut_rx, Ie_eff_tx, Ppl_tx, Ppl_cut_tx;
		const int Ie = 0; /* Not used : Refer to Appendix I of [ITU-T G.113] for the currently recommended values of Ie.*/
		float Ta = 0.0; /* Absolute Delay */
		Ppl_rx = (rxStat.loss+rxStat.discard) * 100.0 / (rxStat.pkt + rxStat.loss);
		Ppl_tx = (txStat.loss+txStat.discard) * 100.0 / (txStat.pkt + txStat.loss);

		float BurstR_rx = 1.0;
		Ie_eff_rx = (Ie + (95 - Ie) * Ppl_rx / (Ppl_rx/BurstR_rx + Bpl));
		int rfactor_rx = 100 - Ie_eff_rx;
		float mos_rx = rfactor_to_mos(rfactor_rx);
		float BurstR_tx = 1.0;
		Ie_eff_tx = (Ie + (95 - Ie) * Ppl_tx / (Ppl_rx/BurstR_tx + Bpl));
		int rfactor_tx = 100 - Ie_eff_tx;
		float mos_tx = rfactor_to_mos(rfactor_tx);

		LOG(logINFO) << __FUNCTION__ <<" rtt:"<< rtcp.rttUsec.mean/1000 <<" mos_lq_tx:"<<mos_tx<<" mos_lq_rx:"<<mos_rx;
		rtt = rtcp.rttUsec.mean/1000;
		test->rtp_stats_json = " \"rtp_stats\":{\"rtt\":"+to_string(rtt)+","
						"\"Tx\":{"
							"\"jitter_avg\": "+to_string(txStat.jitterUsec.mean/1000)+", "
							"\"jitter_max\": "+to_string(txStat.jitterUsec.max/1000)+", "
							"\"pkt\": "+to_string(txStat.pkt)+", "
							"\"kbytes\": "+to_string(txStat.bytes/1024)+", "
							"\"loss\": "+to_string(txStat.loss)+", "
							"\"discard\": "+to_string(txStat.discard)+", "
							"\"mos_lq\": "+to_string(mos_tx)+"} "
						", \"Rx\":{"
							"\"jitter_avg\": "+to_string(rxStat.jitterUsec.mean/1000)+", "
							"\"jitter_max\": "+to_string(rxStat.jitterUsec.max/1000)+", "
							"\"pkt\": "+to_string(rxStat.pkt)+", "
							"\"kbytes\": "+to_string(rxStat.bytes/1024)+", "
							"\"loss\": "+to_string(rxStat.loss)+", "
							"\"discard\": "+to_string(rxStat.discard)+", "
							"\"mos_lq\": "+to_string(mos_rx)+"} "
						"}";
		test->rtp_stats_ready = true;
	} catch (pj::Error e)  {
			LOG(logERROR) <<__FUNCTION__<<" error :" << e.status << std::endl;
	}
}

void TestCall::onStreamCreated(OnStreamCreatedParam &prm) {
	LOG(logDEBUG) <<__FUNCTION__<< " idx["<<prm.streamIdx<<"]\n";
	//pjmedia_stream const *pj_stream = (pjmedia_stream *)&prm.stream;
	//pjmedia_stream_info *pj_stream_info;
	//pjmedia_stream_get_info(pj_stream, pj_stream_info);
}

static pj_status_t record_call(TestCall* call, pjsua_call_id call_id, const char *caller_contact) {
	pj_status_t status = PJ_SUCCESS;
	pjsua_recorder_id recorder_id;
	char rec_fn[1024] = "voice_ref_files/recording.wav";
	CallInfo ci = call->getInfo();
	sprintf(rec_fn,"voice_files/%s_%s_rec.wav", ci.callIdString.c_str(), caller_contact);
	call->test->record_fn = string(&rec_fn[0]);
	const pj_str_t rec_file_name = pj_str(rec_fn);
	status = pjsua_recorder_create(&rec_file_name, 0, NULL, -1, 0, &recorder_id);
	if (status != PJ_SUCCESS) {
		LOG(logINFO) <<__FUNCTION__<<": [error] tecord_call \n";
		return status;
	}
	call->recorder_id = recorder_id;
	LOG(logINFO) <<__FUNCTION__<<": [recorder] created:" << recorder_id << " fn:"<< rec_fn;
	status = pjsua_conf_connect( pjsua_call_get_conf_port(call_id), pjsua_recorder_get_conf_port(recorder_id) );
}

static pj_status_t stream_to_call(TestCall* call, pjsua_call_id call_id, const char *caller_contact ) {
	pj_status_t status = PJ_SUCCESS;
	pjsua_player_id player_id;
	char * fn = new char [call->test->play.length()+1];
	strcpy (fn, call->test->play.c_str());
	const pj_str_t file_name = pj_str(fn);
	status = pjsua_player_create(&file_name, 0, &player_id);
	delete[] fn;
	if (status != PJ_SUCCESS) {
		LOG(logINFO) <<__FUNCTION__<<": [error] creating player\n";
		return status;
	}
	call->player_id = player_id;
	status = pjsua_conf_connect( pjsua_player_get_conf_port(player_id), pjsua_call_get_conf_port(call_id) );
	return status;
}


void TestCall::onCallState(OnCallStateParam &prm) {
	PJ_UNUSED_ARG(prm);

	LOG(logDEBUG) <<__FUNCTION__;
	CallInfo ci = getInfo();

	int uri_prefix = 3; // sip:
	std::string remote_user("");
	std::string local_user("");
	std::size_t pos = ci.localUri.find("@");
	if (ci.localUri[0] == '<')
		uri_prefix++;
	if (pos!=std::string::npos) {
		local_user = ci.localUri.substr(uri_prefix, pos - uri_prefix);
	}
	pos = ci.remoteUri.find("@");
	uri_prefix = 3;
	if (ci.remoteUri[0] != '<')
		uri_prefix++;
	if (pos!=std::string::npos) {
		remote_user = ci.remoteUri.substr(uri_prefix, pos - uri_prefix);
	}
	role = ci.role;

	if (test) {
		pjsip_tx_data *pjsip_data = (pjsip_tx_data *) prm.e.body.txMsg.tdata.pjTxData;
		if (pjsip_data) {
			test->transport = pjsip_data->tp_info.transport->type_name;
			test->peer_socket = pjsip_data->tp_info.dst_name;
			test->peer_socket = test->peer_socket +":"+ std::to_string(pjsip_data->tp_info.dst_port);
		}
		if (test->state != VPT_DONE && test->wait_state && (int)test->wait_state <= (int)ci.state ) {
			test->state = VPT_RUN;
			LOG(logDEBUG) <<__FUNCTION__<<": [test-wait-return]";
		}
		LOG(logINFO) <<__FUNCTION__<<": ["<<getId()<<"]role["<<(ci.role==0?"CALLER":"CALLEE")<<"]id["<<ci.callIdString
                             <<"]["<<ci.localUri<<"]["<<ci.remoteUri<<"]["<< ci.stateText<<"|"<<ci.state<<"]";
		test->call_id = getId();
		test->sip_call_id = ci.callIdString;
	}
	if (test && (ci.state == PJSIP_INV_STATE_DISCONNECTED || ci.state == PJSIP_INV_STATE_CONFIRMED)) {
		std::string res = "call[" + std::to_string(ci.lastStatusCode) + "] reason["+ ci.lastReason +"]";
		test->connect_duration = ci.connectDuration.sec;
		test->setup_duration = ci.totalDuration.sec - ci.connectDuration.sec;
		test->result_cause_code = (int)ci.lastStatusCode;
		test->reason = ci.lastReason;
		if (ci.state == PJSIP_INV_STATE_DISCONNECTED || (test->hangup_duration && ci.connectDuration.sec >= test->hangup_duration) ){
			if (test->state != VPT_DONE) {
				test->update_result();
			}
			if (ci.state == PJSIP_INV_STATE_CONFIRMED){
				CallOpParam prm(true);
				LOG(logINFO) <<__FUNCTION__<<": hangup : call in PJSIP_INV_STATE_CONFIRMED" ;
				hangup(prm);
				LOG(logINFO) <<__FUNCTION__<<": hangup ok";
			}
		}
	}
	// Create player and recorder
	if (ci.state == PJSIP_INV_STATE_CONFIRMED){
		if (test->play_dtmf.length() > 0) {
			dialDtmf(test->play_dtmf);
			LOG(logINFO) <<__FUNCTION__<<": [dtmf]" << test->play_dtmf;
		}
		stream_to_call(this, ci.id, remote_user.c_str());
		if (test->min_mos)
			record_call(this, ci.id, remote_user.c_str());
	}
	if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
		LOG(logINFO) <<__FUNCTION__<<": [Call disconnected]";
		if (player_id != -1) {
			pjsua_player_destroy(player_id);
			player_id = -1;
		}
		if (recorder_id != -1){
			pjsua_recorder_destroy(recorder_id);
			recorder_id = -1;
		}
	}
}


/*
 * TestAccount implementation
 */

void TestAccount::setTest(Test *ptest) {
	test = ptest;
}

TestAccount::TestAccount() {
	test=NULL;
	config=NULL;
	hangup_duration=0;
	max_duration=0;
	ring_duration=0;
	accept_label="-";
	expected_cause_code=200;
}

TestAccount::~TestAccount() {
	LOG(logINFO) << "[Account] is being deleted: No of calls=" << calls.size() ;
}

void TestAccount::onRegState(OnRegStateParam &prm) {
	AccountInfo ai = getInfo();
	LOG(logINFO) << (ai.regIsActive? "[Register] code:" : "[Unregister] code:") << prm.code ;
	if (test) {
		if ( prm.rdata.pjRxData && prm.code != 408 && prm.code != PJSIP_SC_SERVICE_UNAVAILABLE) {
			pjsip_rx_data *pjsip_data = (pjsip_rx_data *) prm.rdata.pjRxData;
			test->transport = pjsip_data->tp_info.transport->type_name;
		}
		std::string res = "registration[" + std::to_string(prm.code) + "] reason["+ prm.reason + "] expiration[" + std::to_string(prm.expiration) +"]";
		test->result_cause_code = (int)prm.code;
		test->reason = prm.reason;
		test->update_result();
	}
}

void TestAccount::onIncomingCall(OnIncomingCallParam &iprm) {
	TestCall *call = new TestCall(this, iprm.callId);

	pjsip_rx_data *pjsip_data = (pjsip_rx_data *) iprm.rdata.pjRxData;
	CallInfo ci = call->getInfo();
	CallOpParam prm;
	AccountInfo acc_inf = getInfo();
	LOG(logINFO) <<__FUNCTION__<<": ["<< acc_inf.uri <<"]["<<call->getId()<<"]from["<<ci.remoteUri<<"]to["<<ci.localUri<<"]id["<<ci.callIdString<<"]";
	if (!call->test) {
		string type("accept");
		LOG(logINFO)<<__FUNCTION__<<": max call duration["<< hangup_duration <<"]";
		call->test = new Test(config, type);
		call->test->hangup_duration = hangup_duration;
		call->test->max_duration = max_duration;
		call->test->ring_duration = ring_duration;
		call->test->expected_cause_code = expected_cause_code;
		call->test->local_user = ci.localUri;
		call->test->remote_user = ci.remoteUri;
		call->test->label = accept_label;
		call->test->sip_call_id = ci.callIdString;
		call->test->transport = pjsip_data->tp_info.transport->type_name;
		call->test->peer_socket = iprm.rdata.srcAddress;
		call->test->state = VPT_RUN;
		call->test->rtp_stats = rtp_stats;
		call->test->code = (pjsip_status_code) code;
		call->test->reason = reason;
		if (wait_state != INV_STATE_NULL)
			call->test->state = VPT_RUN_WAIT;
		LOG(logINFO) <<__FUNCTION__<<"account play:" << play;
		call->test->play = play;
		call->test->play_dtmf = play_dtmf;
	}
	calls.push_back(call);
	config->calls.push_back(call);
	LOG(logINFO) <<__FUNCTION__<<"code:" << code <<" reason:"<< reason;
	prm.statusCode = PJSIP_SC_OK;
	if (ring_duration > 0) {
			prm.statusCode = PJSIP_SC_PROGRESS;
	} else {
		if (reason.size() > 0) prm.reason = reason;
		if (code) prm.statusCode = (pjsip_status_code) code;
	}
	call->answer(prm);
}


/*
 *  Test implementation
 */

Test::Test(Config *config, string type) : config(config), type(type) {
	char now[20] = {'\0'};
	get_time_string(now);
	from="";
	to="";
	wait_state = INV_STATE_NULL;
	state = VPT_RUN;
	start_time = now;
	min_mos = 0.0;
	mos = 0.0;
	expected_cause_code = -1;
	result_cause_code = -1;
	reason = "";
	connect_duration = 0;
	expected_duration = 0;
	setup_duration = 0;
	max_duration = 0;
	ring_duration = 0;
	hangup_duration = 0;
	call_id = 0;
	sip_call_id = "";
	label = "-";
	recording = false;
	playing=false;
	rtp_stats_ready=false;
	rtp_stats=false;
	queued=false;
	LOG(logINFO)<<__FUNCTION__<<LOG_COLOR_INFO<<": New test created:"<<type<<LOG_COLOR_END;
}

void Test::get_mos() {
	std::string reference = "voice_ref_files/reference_8000_12s.wav";
	std::string degraded = "voice_files/" + remote_user + "_rec.wav";
	LOG(logINFO)<<__FUNCTION__<<": [call] mos["<<mos<<"] min-mos["<<min_mos<<"] "<< reference <<" vs "<< record_fn;
}

void jsonify(std::string *str) {
	size_t index = 0;
	while (true) {
		index = str->find("\"", index);
		if (index == std::string::npos) break;
		std::cout << str->substr(index)  << std::endl;
		str->replace(index, 1, "\\\"");
		index += 2;
	}
}

void Test::update_result() {
		char now[20] = {'\0'};
		bool success = false;
		get_time_string(now);
		end_time = now;
		state = VPT_DONE;
		std::string res = "FAIL";

		if (min_mos > 0 && mos == 0) {
				return;
		}
		if (rtp_stats && !rtp_stats_ready) {
			LOG(logINFO)<<__FUNCTION__<<" push_back rtp_stats";
			if (queued) return;
			queued = true;
			config->tests_with_rtp_stats.push_back(this);
			return;
		}

		if (expected_duration && expected_duration != connect_duration) {
			success=false;
		} else if (max_duration && max_duration < connect_duration) {
			success=false;
		} else if(expected_cause_code == result_cause_code && mos >= min_mos) {
			res = "PASS";
			success=true;
		}

		// JSON report
		string jsonFrom = local_user;
		jsonify(&jsonFrom);
		string jsonTo = remote_user;
		jsonify(&jsonTo);
		string jsonCallid = sip_call_id;
		jsonify(&jsonCallid);
		string jsonReason = reason;
		jsonify(&jsonReason);

		config->json_result_count++;
		std::string result_line_json = "{\""+std::to_string(config->json_result_count)+"\": {"
							"\"label\": \""+label+"\", "
							"\"start\": \""+start_time+"\", "
							"\"end\": \""+end_time+"\", "
							"\"action\": \""+type+"\", "
							"\"from\": \""+jsonFrom+"\", "
							"\"to\": \""+jsonTo+"\", "
							"\"result\": \""+res+"\", "
							"\"expected_cause_code\": "+std::to_string(expected_cause_code)+", "
							"\"cause_code\": "+std::to_string(result_cause_code)+", "
							"\"reason\": \""+jsonReason+"\", "
							"\"callid\": \""+jsonCallid+"\", "
							"\"transport\": \""+transport+"\", "
							"\"peer_socket\": \""+peer_socket+"\", "
							"\"duration\": "+std::to_string(connect_duration)+", "
							"\"expected_duration\": "+std::to_string(expected_duration)+", "
							"\"max_duration\": "+std::to_string(max_duration)+", "
							"\"hangup_duration\": "+std::to_string(hangup_duration);
		if (dtmf_recv.length() > 0)
			result_line_json += ", \"dtmf_recv\": \""+dtmf_recv+"\"";
		if (rtp_stats && rtp_stats_ready)
			result_line_json += "," + rtp_stats_json;
		result_line_json += "}}";
		config->result_file.write(result_line_json);
		LOG(logINFO)<<"["<<now<<"]" << result_line_json;
		config->result_file.flush();

		LOG(logINFO)<<" ["<<type<<"]"<<endl;

		// prepare HTML report
		std::string td_style= "style='border-color:#98B4E5;border-style:solid;padding:3px;border-width:1px;'";
		std::string td_hd_style = "style='border-color:#98B4E5;background-color: #EEF2F5;border-style:solid;padding:3px;border-width:1px;'";
		std::string td_small_style="style='padding:1px;width:50%;border-style:solid;border-spacing:0px;border-width:1px;border-color:#98B4E5;text-align:center;font-size:8pt'";
		if (config->testResults.size() == 0){
			std::string headers = "<tr>"
				"<td "+td_hd_style+">label</td>"
				"<td "+td_hd_style+">start/end</td>"
				"<td "+td_hd_style+">type</td><td "+td_hd_style+">result</td>"
				"<td "+td_hd_style+">cause code</td><td "+td_hd_style+">reason</td>"
				"<td "+td_hd_style+">duration</td>"
				"<td "+td_hd_style+">from</td><td "+td_hd_style+">to</td>\r\n";
			config->testResults.push_back(headers);
		}
		std::string mos_color = "green";
		std::string code_color = "green";
		if (expected_cause_code != result_cause_code)
			code_color = "red";
		if (mos < min_mos)
			mos_color = "red";
		if (!success)
			res = "<font color='red'>"+res+"</font>";

		std::string html_duration_table = "<table><tr><td>expected</td><td>max</td><td>hangup</td><td>connect</td></tr><tr>"
			"<td "+td_small_style+">"+std::to_string(expected_duration)+"</td>"
			"<td "+td_small_style+">"+std::to_string(max_duration)+"</td>"
			"<td "+td_small_style+">"+std::to_string(hangup_duration)+"</td>"
			"<td "+td_small_style+">"+std::to_string(connect_duration)+"</td></tr></table>";
		type = type +"["+std::to_string(call_id)+"]transport["+transport+"]<br>peer socket["+peer_socket+"]<br>"+sip_call_id;
		std::string result = "<tr>"
			"<td "+td_style+">"+label+"</td>"
			"<td "+td_style+">"+start_time+"<br>"+end_time+"</td><td "+td_style+">"+type+"</td>"
			"<td "+td_style+">"+res+"</td>"
			"<td "+td_style+">"+std::to_string(expected_cause_code)+"|<font color="+code_color+">"+std::to_string(result_cause_code)+"</font></td>"
			"<td "+td_style+">"+reason+"</td>"
			"<td "+td_style+">"+html_duration_table+"</td>"
			"<td "+td_style+">"+local_user+"</td>"
			"<td "+td_style+">"+remote_user+"</td>"
			"</tr>\r\n";
		config->testResults.push_back(result);
}



/*
 * ResultFile implementation
 */

ResultFile::ResultFile(string name) : name(name) {
	open();
}

bool ResultFile::write(string res) {
	try {
		file << res << "\n";
	} catch (Error & err) {
		LOG(logINFO) <<__FUNCTION__<< "Exception: " << err.info() ;
		return false;
	}
	return true;
}

void ResultFile::flush() {
	file.flush();
}

bool ResultFile::open() {
	file.open(name.c_str(), std::fstream::in | std::fstream::out | std::fstream::app);
	if (file.is_open()) {
		LOG(logINFO) << "JSON result file:" << name << "\n";
	} else {
		std::cerr <<__FUNCTION__<< " [error] test can not open log file :" << name ;
		return false;
	}
}

void ResultFile::close() {
	file.close();
}


/*
 * Config implementation
 */

Config::Config(string result_fn) : result_file(result_fn), action(this) {
		tls_cfg.ca_list = "tls/ca_list.pem";
		tls_cfg.private_key = "tls/key.pem";
		tls_cfg.certificate = "tls/certificate.pem";
		tls_cfg.verify_server = 0;
		tls_cfg.verify_client = 0;
		json_result_count = 0;
}

void Config::log(std::string message) {
	LOG(logINFO) <<"[timestamp]"<< message ;
}

Config::~Config() {
	result_file.close();
}

void Config::removeCall(TestCall *call) {
	for (auto it = calls.begin(); it != calls.end(); ++it) {
		if (*it == call) {
			calls.erase(it);
			break;
		}
	}
	delete call;
}

void Config::createDefaultAccount() {
	AccountConfig acc_cfg;
	acc_cfg.idUri = "sip:default";
	TestAccount *acc = createAccount(acc_cfg);
	acc->play = default_playback_file;
	LOG(logINFO) <<__FUNCTION__<<" created:"<<default_playback_file;
}

TestAccount* Config::createAccount(AccountConfig acc_cfg) {
	TestAccount *account = new TestAccount();
	accounts.push_back(account);
	account->config = this;
	account->create(acc_cfg);
	AccountInfo acc_inf = account->getInfo();
	LOG(logINFO) <<__FUNCTION__<< ": ["<< acc_inf.id << "]["<<acc_inf.uri<<"]";
	return account;
}

TestAccount* Config::findAccount(std::string account_name) {
	if (account_name.compare(0, 1, "+") == 0)
		account_name.erase(0,1);
	for (auto account : accounts) {
		AccountInfo acc_inf = account->getInfo();
		int proto_length = 4; // "sip:"
		if (acc_inf.uri.compare(0, 4, "sips") == 0)
			proto_length = 5;
		LOG(logINFO) <<__FUNCTION__<< ": [searching account]["<< acc_inf.id << "]["<<acc_inf.uri<<"]["<<acc_inf.uri.substr(proto_length)<<"]<>["<<account_name<<"]";
		if (acc_inf.uri.compare(proto_length, account_name.length(), account_name) == 0 ) {
			LOG(logINFO) <<__FUNCTION__<< ": found account id["<< acc_inf.id <<"] uri[" << acc_inf.uri <<"]";
			return account;
		}
	}
	return nullptr;
}

bool Config::process(std::string p_configFileName, std::string p_jsonResultFileName) {
	ezxml_t xml_actions, xml_action, xml_xhdr;
	configFileName = p_configFileName;
	ezxml_t xml_conf = ezxml_parse_file(configFileName.c_str());
	xml_conf_head = xml_conf; // saving the head if the linked list

	if(!xml_conf){
		LOG(logINFO) <<__FUNCTION__<< "[error] test can not load file :" << configFileName ;
		return false;
	}

	for (xml_actions = ezxml_child(xml_conf, "actions"); xml_actions; xml_actions=xml_actions->next) {
		LOG(logINFO) <<__FUNCTION__<< " ===> " << xml_actions->name;
		string for_var = ezxml_attr(xml_actions, "for");
		int start = 0;
		int stop = 1;
		int step = 1;
		if (for_var) {
			start = ezxml_attr(xml_actions, "start");
			if (start) start = atoi(start);
			sttop = ezxml_attr(xml_actions, "stop");
			if (stop) stop = atoi(start);
			step = ezxml_attr(xml_actions, "step");
			if (step) start = atoi(start);
		}
		for (int i = start; i < stop; i++) {
			for (xml_action = ezxml_child(xml_actions, "action"); xml_action; xml_action=xml_action->next) {
				const char * val = ezxml_attr(xml_action,"type");
				if (!val) {
					LOG(logERROR) <<__FUNCTION__<<" invalid action !";
					continue;
				}
				string action_type = ezxml_attr(xml_action,"type");;
				LOG(logINFO) <<__FUNCTION__<< " ===> action/" << action_type;
				vector<ActionParam> params = action.get_params(action_type);
				if (params.size() == 0) {
					LOG(logERROR) <<__FUNCTION__<< ": params not found for action:" << action_type << std::endl;
					continue;
				}
				//TOOD: set iterator param on each action
				for (auto &param : params) {
					action.set_param(param, ezxml_attr(xml_action, param.name.c_str()));
				}
				if ( action_type.compare("wait") == 0 ) action.do_wait(params);
				else if ( action_type.compare("call") == 0 ) {
					SipHeaderVector x_hdrs = SipHeaderVector();
					for (xml_xhdr = ezxml_child(xml_action, "x-header"); xml_xhdr; xml_xhdr=xml_xhdr->next) {
						SipHeader sh = SipHeader();
						sh.hName = ezxml_attr(xml_xhdr, "name");
						sh.hValue = ezxml_attr(xml_xhdr, "value");
						if (sh.hValue.compare(0, 7, "VP_ENV_") == 0) {
							sh.hValue = action.get_env(sh.hValue);
						}
						x_hdrs.push_back(sh);
					}
					action.do_call(params, x_hdrs);
				}
				else if ( action_type.compare("accept") == 0 ) action.do_accept(params);
				else if ( action_type.compare("register") == 0 ) action.do_register(params);
				else if ( action_type.compare("alert") == 0 ) action.do_alert(params);
			}
		}
	}
}


/*
 * Alert implementation
 */

Alert::Alert(Config * p_config){
	curl = curl_easy_init();
	config = p_config;
}
void Alert::prepare(void){
//	std::string date = "Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n";
//	upload_data.payload_content.push_back(date);
	alert_server_url = config->alert_server_url;
	std::string to = "To: <"+config->alert_email_to+">\r\n";
	upload_data.payload_content.push_back(to);
	std::string from = "From: <"+config->alert_email_from+">\r\n";
	upload_data.payload_content.push_back(from);
	std::string messageId = "Message-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@rfcpedant.example.org>\r\n";
	upload_data.payload_content.push_back(messageId);
	std::string subject = "Subject: VoIP Patrol test report\r\n";
	upload_data.payload_content.push_back(subject);
	std::string content_type = "Content-type: text/html\r\n";
	upload_data.payload_content.push_back(content_type);
	std::string bodySeparator = "\r\n";
	upload_data.payload_content.push_back(bodySeparator);

	std::string tb_style = "style='font-size:8pt;font-family:\"DejaVu Sans\",Verdana;"
                                       "border-collapse:collapse;border-spacing:0px;"
                                       "border-style:solid;border-width:1px;text-align:center;'";
	std::string html_start = "<html><table "+tb_style+">";
	upload_data.payload_content.push_back(html_start);
	for (auto & testResult : config->testResults) {
		upload_data.payload_content.push_back(testResult);
	}
	std::string html_end = "</table></html>\n\r";
	upload_data.payload_content.push_back(html_end);
}

void Alert::send(void) {
	CURLcode res = CURLE_OK;
	struct curl_slist *recipients = NULL;
	upload_data.lines_read = 0;
	LOG(logINFO) <<__FUNCTION__<< " smtp" << config->alert_server_url;
	if (config->alert_server_url.empty() || config->alert_email_to.empty() || config->alert_email_from.empty())
		return;

	prepare();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, alert_server_url.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, alert_email_from.c_str());
		recipients = curl_slist_append(recipients, config->alert_email_to.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, &Alert::payload_source);
		curl_easy_setopt(curl, CURLOPT_READDATA, &upload_data);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		res = curl_easy_perform(curl);
		if(res != CURLE_OK)
		std::cerr << "curl_easy_perform() failed: "  << curl_easy_strerror(res) << alert_server_url <<"\n";
		curl_slist_free_all(recipients);
		curl_easy_cleanup(curl);
	}
	LOG(logINFO) << "email alert sent...\n";
}

size_t Alert::payload_source(void *ptr, size_t size, size_t nmemb, void *userp) {
	upload_data_t *upload_data = (upload_data_t *)userp;
	const char *data;

	if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
		return 0;
	}

	if(upload_data->lines_read >= upload_data->payload_content.size())
		return 0;
	data = upload_data->payload_content[upload_data->lines_read].c_str();
	if(data) {
		size_t len = strlen(data);
		memcpy(ptr, data, len);
		upload_data->lines_read++;
		return len;
	}

	return 0;
}

/*
*   VoipPatrolEndpoint implementation
*/

void VoipPatrolEnpoint::onSelectAccount(OnSelectAccountParam &param) {
	LOG(logDEBUG) <<__FUNCTION__<<" account_index:" << param.accountIndex << "\n" << param.rdata.wholeMsg ;
	pjsip_rx_data *pjsip_data = (pjsip_rx_data *) param.rdata.pjRxData;
	pjsip_to_hdr* to_hdr = (pjsip_to_hdr*) pjsip_msg_find_hdr(pjsip_data->msg_info.msg, PJSIP_H_TO, NULL);
	const pjsip_sip_uri* sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(to_hdr->uri);
	std::string to(sip_uri->user.ptr, sip_uri->user.slen);
	LOG(logINFO) <<__FUNCTION__<<" to:" << to ;

	TestAccount* account = config->findAccount(to);
	if (!account) return;

	AccountInfo acc_info = account->getInfo();
	param.accountIndex = acc_info.id;
}


int main(int argc, char **argv){
	int ret = 0;

	pjsip_cfg_t *pjsip_config = pjsip_cfg();
	std::cout <<"pjsip_config->tsx.t1 :" << pjsip_config->tsx.t1 <<"\n";
	// pjsip_config->tsx.t1 = 250;
	// pjsip_config->tsx.t2 = 250;
	// pjsip_config->tsx.t4 = 1000;

	pjsip_cfg()->endpt.disable_secure_dlg_check = 1;

	VoipPatrolEnpoint ep;

	std::string conf_fn = "conf.xml";
	std::string log_fn = "";
	std::string log_test_fn = "results.json";
	int port = 5070;
	int log_level_console = 2;
	int log_level_file = 10;
	Config config(log_test_fn);

	ep.config = &config;

	// command line argument
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			std::cout <<"\n"<< argv[0] <<"                                \n"\
            " -v --vesion                       voip_patrol version       \n"\
            " --log-level-file <0-10>           file log level            \n"\
            " --log-level-console <0-10>        console log level         \n"\
            " -p --port <5060>                  local port                \n"\
            " -c,--conf <conf.xml>              XML scenario file         \n"\
            " -l,--log <logfilename>            voip_patrol log file name \n"\
            " -o,--output <result.json>         json result file name     \n"\
            " --tls-calist <path/file_name>     TLS CA list (pem format)     \n"\
            " --tls-privkey <path/file_name>    TLS private key (pem format) \n"\
            " --tls-cert <path/file_name>       TLS certificate (pem format) \n"\
            " --tls-verify-server               TLS verify server certificate \n"\
            " --tls-verify-client               TLS verify client certificate \n"\
			"                                                             \n";
			return 0;
		} else if ( (arg == "-v") || (arg == "--version") ) {
			std::cout <<"version: voip_patrol "<<VERSION<<std::endl;
			return 0;
		} else if ( (arg == "-c") || (arg == "--conf") ) {
			if (i + 1 < argc) {
				conf_fn = argv[++i];
			}
		} else if ( (arg == "--log-level-file") ) {
			if (i + 1 < argc) {
				log_level_file = atoi(argv[++i]);
			}
		} else if ( (arg == "--log-level-console") ) {
			if (i + 1 < argc) {
				log_level_console = atoi(argv[++i]);
			}
		} else if ( (arg == "-l") || (arg == "--log")) {
			if (i + 1 < argc) {
				log_fn = argv[++i];
			}
		} else if (arg == "--tls-privkey") {
			config.tls_cfg.private_key = argv[++i];
		} else if (arg == "--tls-verify-client") {
			config.tls_cfg.verify_client = 1;
		} else if (arg == "--tls-verify-server") {
			config.tls_cfg.verify_server = 1;
		} else if (arg == "--tls-calist") {
			config.tls_cfg.ca_list = argv[++i];
		} else if (arg == "--tls-cert") {
			config.tls_cfg.certificate = argv[++i];
		} else if ( (arg == "-p") || (arg == "--port")) {
			if (i + 1 < argc) {
				port = atoi(argv[++i]);
			}
		} else if ( (arg == "-o") || (arg == "--output")) {
			if (i + 1 < argc) {
				log_test_fn = argv[++i];
			}
		}
	}

	FILELog::ReportingLevel() = (TLogLevel)log_level_console;
	if ( log_fn.length() > 0 ) {
		FILELog::ReportingLevel() = logDEBUG3;
		FILE* log_fd = fopen(log_fn.c_str(), "w");
		Output2FILE::Stream() = log_fd;
	}
	std::cout << "\n* * * * * * *\n "
		"voip_patrol version: "<<VERSION<<"\n"
		"configuration: "<<conf_fn<<"\n"
		"log file: "<<log_fn<<"\n"
		"output file: "<<log_test_fn<<"\n"
		"* * * * * * *\n";

	TransportConfig tcfg;
	try {
		ep.libCreate();
		EpConfig ep_cfg;
		ep_cfg.uaConfig.maxCalls = 1000;
		ep_cfg.logConfig.level = log_level_file;
		ep_cfg.logConfig.consoleLevel = log_level_console;
		std::string pj_log_fn =  "pjsua_" + std::to_string(port) + ".log";
		ep_cfg.logConfig.filename = pj_log_fn.c_str();
		ep_cfg.medConfig.ecTailLen = 0; // disable echo canceller
		ep_cfg.medConfig.noVad = 1;

		ep.libInit( ep_cfg );
		// pjsua_set_null_snd_dev() before calling pjsua_start().

		// TCP and UDP transports

		tcfg.port = port;
		config.transport_id_tcp = ep.transportCreate(PJSIP_TRANSPORT_TCP, tcfg);
		tcfg.port = port;
		config.transport_id_udp = ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);
	} catch (Error & err) {
		LOG(logINFO) <<__FUNCTION__<<": Exception: " << err.info() ;
		return 1;
	}

	try {
		// TLS transport
		tcfg.port = port+1;
		// Optional, set CA/certificate/private key files.
		tcfg.tlsConfig.CaListFile = config.tls_cfg.ca_list;
		tcfg.tlsConfig.certFile = config.tls_cfg.certificate;
		tcfg.tlsConfig.privKeyFile = config.tls_cfg.private_key;
		tcfg.tlsConfig.verifyServer = config.tls_cfg.verify_server;
		tcfg.tlsConfig.verifyClient = config.tls_cfg.verify_client;
		// Optional, set ciphers. You can select a certain cipher/rearrange the order of ciphers here.
		// tcfg.ciphers = ep->utilSslGetAvailableCiphers();
		config.transport_id_tls = ep.transportCreate(PJSIP_TRANSPORT_TLS, tcfg);
		LOG(logINFO) <<__FUNCTION__<<": TLS supported ";
	} catch (Error & err) {
		config.transport_id_tls = -1;
		LOG(logINFO) <<__FUNCTION__<<": Exception: TLS not supported, see README. " << err.info() ;
	}

	try {
		// load config and execute test
		pjsua_set_null_snd_dev();
		ep.libStart();

		config.createDefaultAccount();
		config.process(conf_fn, log_test_fn);

		LOG(logINFO) <<__FUNCTION__<<": wait complete all...";
		vector<ActionParam> params = config.action.get_params("wait");
		config.action.set_param_by_name(&params, "complete");
		config.action.do_wait(params);

		LOG(logINFO) <<__FUNCTION__<<": checking alerts...";

		// send email reporting
		Alert alert(&config);
		alert.send();

		LOG(logINFO) <<__FUNCTION__<<": hangup all calls..." ;
		ep.hangupAllCalls();

		ret = PJ_SUCCESS;
	} catch (Error &err) {
		LOG(logINFO) <<__FUNCTION__<<": Exception: " << err.info() ;
		ret = 1;
	}


	try {
		ep.libDestroy();
	} catch (Error &err) {
		LOG(logINFO) <<__FUNCTION__<<": Exception: " << err.info() ;
		ret = 1;
	}

	if (ret == PJ_SUCCESS) {
		LOG(logINFO) <<__FUNCTION__<<": Success" ;
	} else {
		LOG(logINFO) <<__FUNCTION__<<": Error Found" ;
	}

	LOG(logINFO) <<__FUNCTION__<<": exiting !" ;
	return ret;
}


