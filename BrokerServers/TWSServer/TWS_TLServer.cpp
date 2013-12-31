#include "StdAfx.h"
#include "TWS_TLServer.h"
#include "Util.h"
#include "Execution.h"
#include <fstream>
#include "IBUtil.h"
#include "TLBar.h"

//ILDEBEGIN
#include <iostream>
//ILDEEND


namespace TradeLibFast
{
	const char* CONFIGFILE = "TwsServer.Config.txt";
	const char* LINE = "-------------------------------";

	TWS_TLServer::TWS_TLServer(void)
	{
		//BEGINILDE
		CString clientname_ = "";
		//ENDILDE
		IGNOREERRORS = false;
		_currency = CString("USD");
		noverb = true;
		histBarRTH = 0;
		histBarWhatToShow = CString("TRADES");
		PROGRAM = "TWSServer";
		nextIBID = 1;
	}

	void TWS_TLServer::v(CString msg)
	{
		if (noverb) return;
		D(msg);
	}

	TWS_TLServer::~TWS_TLServer(void)
	{
		// cancel subscriptions
		for (size_t i = 0; i<stockticks.size(); i++)
		{
			D2("~TWS_TLServer: cancelMktData");
			this->m_link[this->validlinkids[0]]->cancelMktData((TickerId)i);
		}

		// disconnect from IB and unallocate memory
		for (size_t i = 0; i<m_link.size(); i++)
		{
			D2("~TWS_TLServer: eDisconnect");
			m_link[i]->eDisconnect();
			delete m_link[i];
		}

	}

	void TWS_TLServer::Start(void)
	{
		TLServer_WM::Start();

		CString m;
		m.Format("Reading config from: %s", CONFIGFILE);	// dimon: we also show path to current config
		D(m);

		std::ifstream file;
		file.open(CONFIGFILE);
		int sessionid = 0;
		const int ss = 200;
		char skip[ss];
		const int ds = 20;
		char data[ds];
		file.getline(skip,ss);
		file.getline(data,ds);
		this->FIRSTSOCKET = atoi(data);
		file.getline(skip,ss);
		file.getline(data,ds);
		int maxsockets = atoi(data); // get the # of sockets first
		file.getline(skip,ss);
		file.getline(data,ds);
		sessionid = atoi(data); // get the session id next 
		file.getline(skip,ss);
		file.getline(data,ds); 

		_currency = CString(data); // get currency

		m.Format("    - Using currency: %s",_currency);
		D(m);

		// get verbosity
		file.getline(skip,ss);
		file.getline(data,ds);
		noverb = 0==atoi(data);
		// get historical bar type
		file.getline(skip,ss);
		file.getline(data,ds);
		histBarWhatToShow = CString(data);

		m.Format("    - Using bar price data: %s",histBarWhatToShow);
		D(m);

		// get historical bar regular trading hours
		file.getline(skip,ss);
		file.getline(data,ds);
		histBarRTH = atoi(data);

		if (histBarRTH)
			D("    - Using bar data from regular trading hours.");
		else
			D("    - Using all bar data available.");

		file.getline(skip,ss);
		file.getline(data,ds);
		fillnotifyfullsymbol = atoi(data)==1;
		if (fillnotifyfullsymbol)
			D("    - Using full quoted symbol on fill notification.");
		else
			D("    - Using only executed symbol on fill notification.");


		file.close();
		if (!noverb)
			D("    - verbosity is on");
		if (sessionid!=0)
		{
			D("    - Orders can only be received from this machine.");
			D(CString("To change this, see ")+CONFIGFILE);
		}
		m.Format("Looking for %i TWS instances...",maxsockets);
		D(m);

		D(CString(LINE));
		InitSockets(maxsockets,sessionid);
		D(CString(LINE));

		m.Format("Found %i of %i.",this->validlinkids.size(),maxsockets);
		D(m);

		D(CString("For more instances, change value in: ")+CONFIGFILE);

		m.Format("Found accounts: %s",gjoin(accts,","));
		D(m);
	}


	void TWS_TLServer::InitSockets(int maxsockets, int clientid)
	{

		// start socket connections that attempt to connect
		// to a specified number of [already running] TWS instances, starting at FIRSTSOCKET
		m_nextsocket = FIRSTSOCKET;
		IGNOREERRORS = true;


		for (int idx = 0; idx<maxsockets; idx++)
		{
			m_link.push_back(new EClientSocket(this)); // create socket instance
			const char* host = _T("");
			CString msg;

			if (this->TLDEBUG_LEVEL >= 2)
			{
				CString dbg_msg;
				dbg_msg.Format("trying to eConnect(%s, %d, clientid=%d) NOTE: for clientid==0 there will be auto open order notifications (even for orders created in TWS manually!)", host, m_nextsocket, clientid);
				D2(dbg_msg);
			}

			bool good = m_link[idx]->eConnect(host, m_nextsocket, clientid); // connect

			int setServerLogLevelValue=5;

			if (this->TLDEBUG_LEVEL >= 2)
			{
				CString dbg_msg;
				dbg_msg.Format("setServerLogLevel(%d)", setServerLogLevelValue);
				D2(dbg_msg);
			}

			m_link[idx]->setServerLogLevel(setServerLogLevelValue);
			if (clientid==0)
			{
				D2("reqAutoOpenOrders(true) <-- enable order notifications");
				m_link[idx]->reqAutoOpenOrders(true); // enable order notifications
			}


			D2("reqAccountUpdates(true,\"\") <-- get account names for this link (note - 2nd param is empty, which supposed to be an account code");
			m_link[idx]->reqAccountUpdates(true, _T("")); // get account names for this link

			D2("TwsConnectionTime() - this call is here to test if we're connected to TWS");
			CString time = m_link[idx]->TwsConnectionTime();
			msg.Format(" port: %i",m_nextsocket);
			if (time.GetLength()>0)
			{
				D(CString("Found instance at")+msg + " connection_time: "+time);
				this->validlinkids.push_back(idx);
			}
			else D(CString("Nothing found at")+msg);
			m_nextsocket++;
		}
		IGNOREERRORS = false;
	}

	std::vector<int> TWS_TLServer::GetFeatures()
	{
		std::vector<int> f;
		f.push_back(SENDORDERMODIFY);
		f.push_back(SENDORDER);
		f.push_back(BROKERNAME);
		f.push_back(REGISTERCLIENT);
		f.push_back(REGISTERSTOCK);
		f.push_back(ACCOUNTREQUEST);
		f.push_back(ACCOUNTRESPONSE);
		f.push_back(ORDERCANCELREQUEST);
		f.push_back(ORDERCANCELRESPONSE);
		f.push_back(ORDERNOTIFY);
		f.push_back(EXECUTENOTIFY);
		f.push_back(TICKNOTIFY);
		f.push_back(SENDORDERMARKET);
		f.push_back(SENDORDERLIMIT);
		f.push_back(SENDORDERSTOP);
		f.push_back(SENDORDERTRAIL);
		f.push_back(LIVEDATA);
		f.push_back(LIVETRADING);
		f.push_back(SIMTRADING);
		f.push_back(POSITIONRESPONSE);
		f.push_back(POSITIONREQUEST);
		f.push_back(BARREQUEST);
		f.push_back(BARRESPONSE);
		//ILDEBEGIN
		f.push_back(DOMREQUEST);
		//ILDEEND
		return f;
	}

	void gettime(int tltime, std::vector<int>& r)
	{
		int sec = tltime % 100;
		r.push_back(sec);
		int n = (tltime - sec)/100;
		int min = n % 100;
		r.push_back(min);
		n = (n-min)/100;
		r.push_back(n);
	}
	void getdate(int tldate, std::vector<int>& r)
	{
		int day = tldate % 100;
		r.push_back(day);
		int n = (tldate - day)/100;
		int month = n % 100;
		r.push_back(month);
		n = (n - month)/100;
		r.push_back(n);
	}
	int TWS_TLServer::UnknownMessage(int MessageType, CString msg)
	{
		switch (MessageType)
		{
		case BARREQUEST:
			{
				// parse bar request
				BarRequest br = BarRequest::Deserialize(msg);
				if (!br.isValid()) break;
				// get contract
				Contract contract;
				TLSecurity sec = TLSecurity::Deserialize(br.Symbol);
				getcontract(br.Symbol,_currency,sec.dest,&contract);
				// get request duration
				std::vector<int> st;
				std::vector<int> et;
				std::vector<int> sd;
				std::vector<int> ed;
				gettime(br.StartTime,st);
				getdate(br.StartDate,sd);
				gettime(br.EndTime,et);
				getdate(br.EndDate,ed);
				CTime t1 = CTime(sd[2],sd[1],sd[0],st[2],st[1],st[0]);
				CTime t2 = CTime(ed[2],ed[1],ed[0],et[2],et[1],et[0]);
				CTimeSpan ts = t2 -t1;
				bool usesec = ts.GetTotalSeconds()<86400;
				bool usedays = ts.GetDays()<30;
				int weeks = (int)(((double)ts.GetDays())/(double)7);

				CString edt;
				edt.Format("%i %i:%i:%i",br.EndDate,et[2],et[1],et[0]);
				// get duration
				CString dur;
				if (usesec)
					dur.Format("%i S",ts.GetTotalSeconds());
				else if (usedays)
					dur.Format("%i D",ts.GetDays());
				else 
					dur.Format("%i W",weeks);
				// get bar size
				CString bs;
				if (br.Interval==1)
					bs = "1 sec";
				else if (br.Interval<60)
					bs.Format("%i secs",br.Interval);
				else if (br.Interval==60)
					bs = "1 min";
				else if (br.Interval<3600)
					bs.Format("%i mins",(int)(br.Interval/60));
				else if (br.Interval==3600)
					bs = "1 hour";
				else if (br.Interval<86400)
					bs.Format("%i hours",(int)(br.Interval/3600));
				else if (br.Interval==86400)
					bs = "1 day";
				// save request
				histBarSymbols.push_back(br);
				// notify
				CString m;
				m.Format("Bar request: %s end date: %s dur: %s size: %s",br.Symbol,edt,dur,bs);
				D(m);

				// request data
				D2("reqHistoricalData");
				this->m_link[this->validlinkids[0]]->reqHistoricalData((TickerId)histBarSymbols.size()-1,contract,edt,dur,bs,histBarWhatToShow,histBarRTH,2);

			}
			break;
		}
		return UNKNOWN_MESSAGE;
	}

	void TWS_TLServer::historicalData(TickerId reqId, const CString& datestring, double open, 
		double high, double low,double close, int volume, 
		int barCount, double WAP, int hasGaps) 
	{
		D1("TWS_TLServer::historicalData");

		// get date from bar
		CString dates = CString(datestring);
		int64 unix = _atoi64(dates.GetBuffer());
		CTime ct(unix);
		int date;
		int time;
		
		// workaround for different date formats that's returned depending on barsize setting (daily vs intraday)
		// should work until the year 3155 or so
		if (ct.GetYear() > 1970)
		{
			date = (ct.GetYear() * 10000) + (ct.GetMonth() * 100) + ct.GetDay();
			time = (ct.GetHour() * 10000) + (ct.GetMinute() * 100) + ct.GetSecond();
		}
		else
		{
			date = unix;
			time = 0;
		}

		// get request
		BarRequest br = histBarSymbols[reqId];
		// build bar
		TLBar b;
		b.symbol = br.Symbol;
		b.open = open;
		b.high = high;
		b.close = close;
		b.low = low;
		b.Vol = volume;
		b.interval = br.Interval;
		b.time = time;
		b.date = date;
		// send bar to requesting client
		TLSend(BARRESPONSE,TLBar::Serialize(b),br.Client);




	}

	int TWS_TLServer::findTLOrder(int64 tlid)
	{
		for (size_t i = 0; i < tlorders.size(); i++)
		{
			if (tlorders[i].id == tlid) return i;
		}
		return -1;
	}

	OrderId TWS_TLServer::TL2IBID(int64 tlid)
	{
		int pos = findTLOrder(tlid);
		if (pos >= 0) return iborders[pos];
		return 0;
	}

	int TWS_TLServer::findIBOrder(OrderId ibid)
	{
		for (size_t i = 0; i < iborders.size(); i++)
		{
			if (iborders[i] == ibid) return i;
		}
		return -1;
	}

	int64 TWS_TLServer::IB2TLID(OrderId ibid)
	{
		int pos = findIBOrder(ibid);
		if (pos >= 0) return tlorders[pos].id;
		return 0;
	}

	int64 TWS_TLServer::saveOrder(OrderId ibid, CString acct)
	{
		int64 tlid = IB2TLID(ibid);
		// don't save if it's already been saved
		if (tlid!=0)
			return tlid;
		// if no id, auto-assign one
		tlid = GetTickCount(); 
		// save relationship
		tlorders.push_back(TLOrderInfo(tlid, acct, true));
		iborders.push_back(ibid);
		// return tradelink id
		return tlid;
	}

	OrderId TWS_TLServer::newOrder(int64 tlid,CString acct)
	{
		if (tlid==0) tlid = GetTickCount(); // if no id, auto-assign one
		OrderId ibid = TL2IBID(tlid);
		if (ibid!=0) return ibid; // id already exists
		ibid = getNextOrderId(acct);
		tlorders.push_back(TLOrderInfo(tlid, acct, false));
		iborders.push_back(ibid);
		return ibid;
	}

	int TypeFromExchange(CString ex)
	{
		if ((ex=="GLOBEX")|| (ex=="NYMEX")||(ex=="CFE"))
			return FUT;
		else if ((ex=="NYSE")||(ex=="NASDAQ")||(ex=="ARCA"))
			return STK;
		else if (ex=="IDEALPRO") // dimon: trying to subscribe to forex CAD.JPY (exchange = "IDEALPRO")
			return CASH;

		// default to STK if not sure
		return 0;

	}

	vector<int> specsymid;
	vector<CString> specsym;
	const int NOTSPECIAL = -1;
	int specid(int id)
	{
		for (int i = 0; i<(int)specsymid.size(); i++)
			if (specsymid[i]==id) return i;
		return NOTSPECIAL;
	}

	CString lastcontract;
	void TWS_TLServer::pcont(Contract* c)
	{
		CString m;
		m.Format("sym: %s local: %s currency: %s ex: %s primaryex: %s security: %s strike: %f expiry: %s",c->symbol,c->localSymbol,c->currency,c->exchange,c->primaryExchange,c->secType,c->strike,c->expiry);
		lastcontract = m;
		v(m);
	}

	CString lastorder;
	void TWS_TLServer::pord(Order* o)
	{
		CString m;
		m.Format("size: %i price: %f stop: %f",o->totalQuantity,o->lmtPrice,o->auxPrice);
		lastorder = m;
		v(m);
	}

	void TWS_TLServer::getcontract(CString symbol, CString currency, CString exchange,Contract* contract)
	{
		TLSecurity tmpsec = TLSecurity::Deserialize(symbol);
		if (symbol.FindOneOf(" ")!=-1)
		{
			contract->localSymbol = tmpsec.sym;
			// options stuff
			if (tmpsec.isCall()|| tmpsec.isPut())
			{
				CString expire;
				expire.Format("%i",tmpsec.date);
				contract->expiry = expire;
				contract->strike = tmpsec.strike;
				contract->right = tmpsec.details;
				//contract->currency = _currency;			
			}
			if (tmpsec.hasDest())
				contract->exchange = tmpsec.dest;
			if (tmpsec.hasType())
				contract->secType = tmpsec.SecurityTypeName(tmpsec.type);
			else if (tmpsec.hasDest())
				contract->secType = TLSecurity::SecurityTypeName(TypeFromExchange(tmpsec.dest));
		}
		else 
		{
			contract->localSymbol = tmpsec.sym;
			contract->exchange = exchange;
			contract->secType = tmpsec.SecurityTypeName(tmpsec.type);

		}
		if (tmpsec.type==CASH)
		{
			// remove base currency from symbol
			CString cpy = CString(tmpsec.sym);
			int pidx = cpy.Find(CString("."));
			if (pidx!=-1)
			{

				cpy.Delete(pidx,cpy.GetLength()-pidx);
			}
			contract->symbol = cpy;
			contract->currency = truncateat(tmpsec.sym,CString("."));
		}
		else
			contract->currency = currency;
		// if we dont' have exchange or security type, guess defaults
		if (contract->exchange=="")
			contract->exchange= "SMART";
		if (contract->secType=="")
			contract->secType = "STK";
		contract->symbol = tl2ibspace(contract->symbol);
		contract->localSymbol = tl2ibspace(contract->localSymbol);
		if (tmpsec.type== FUT)
		{
			CString cpy(contract->localSymbol);
			contract->symbol = cpy.Left(cpy.GetLength()-2);
		}
	}

	int TWS_TLServer::SendOrder(TLOrder o)
	{
		// check our order
		if (o.symbol=="") 
			return UNKNOWN_SYMBOL;
		if (!o.isValid()) 
			return INVALID_ORDERSIZE;
		if (!linktest())
			return BROKERSERVER_NOT_FOUND;


		// create broker-specific objects here
		Order* order(new Order);

		order->auxPrice = o.isTrail() ? o.trail : o.stop;
		order->lmtPrice = o.price;
		order->orderType = (o.isStop() && o.isLimit()) ? "STPLMT" : (o.isStop()) ? "STP" : (o.isLimit() ? "LMT" : (o.isTrail() ? "TRAIL" : "MKT"));
		order->totalQuantity = (long)abs(o.size);
		order->action = (o.side) ? "BUY" : "SELL";
		order->account = o.account;
		order->tif = o.TIF;
		order->outsideRth = true;
		order->orderId = newOrder(o.id,o.account);
		order->transmit = true;



		Contract* contract(new Contract);
		getcontract(o.symbol,o.currency,o.exchange,contract);
		pcont(contract);
		pord(order);		

		// get the TWS session associated with our account
		EClient* client;
		if (o.account=="") // if no account specified, get default
			client = m_link[this->validlinkids[0]];
		else // otherwise get the session our account is logged into
			client	= GetOrderSink(o.account);



		// place our order
		if (client!=NULL)
			client->placeOrder(order->orderId,*contract,*order);


		delete order;
		delete contract;

		return OK;
	}

	int TWS_TLServer::BrokerName(void)
	{
		return InteractiveBrokers;
	}



	bool TWS_TLServer::hasAccount(CString account)
	{
		if (account=="") return false; // don't match empty strings
		for (size_t i = 0; i<accts.size(); i++)
			if (accts[i].Find(account)!=-1) return true;
		return false;
	}


	void TWS_TLServer::updateAccountValue( const CString &key, const CString &val,
		const CString &currency, const CString &accountName) 
	{
		D1("TWS_TLServer::updateAccountValue key: " + key + ", val: " + val);
		// make sure we don't have this account already
		if (!hasAccount(accountName))
		{
			accts.push_back(accountName); // save the account name
			// get socket for this account
			EClient* socket = m_link[validlinkids[accts.size()-1]];
			// request updates so we get portfolio data
			socket->reqAccountUpdates(true,accountName);
		}
	}

	void TWS_TLServer::managedAccounts(const CString& accountsList)
	{
		D1("TWS_TLServer::managedAccounts");
		CString msg(accountsList);
		accts.push_back(msg); //save the list of FA accounts
	}

	int TWS_TLServer::AccountResponse(CString clientname)
	{
		std::vector<CString> all;
		for (size_t i = 0; i<accts.size(); i++)
		{
			if (accts[i].Find(",")!=-1) // look for multiple accounts
				all.push_back(accts[i]); // if only one, save it
			else
				gsplit(accts[i],",",all); // otherwise save em all
		}
		CString s = gjoin(all,","); // join em back together
		TLSend(ACCOUNTRESPONSE,s,clientname); // send the whole list
		return OK;
	}

	EClient* TWS_TLServer::GetOrderSink(CString account)
	{
		for (size_t i = 0; i<accts.size(); i++)
			if (accts[i].Find(account)!=-1)
				return m_link[this->validlinkids[i]];
		return NULL;
	}



	void TWS_TLServer::nextValidId( OrderId orderId) 
	{ 
		D1("TWS_TLServer::nextValidId");
		if (orderId > nextIBID) nextIBID = orderId; // pick up maximum id among all accounts
	}

	OrderId TWS_TLServer::getNextOrderId(CString account)
	{
		return nextIBID++;
	}




	std::vector<int> ordercache;

	bool newOrder(OrderId id)
	{
		size_t count = ordercache.size();
		bool neworder = false;
		for (size_t i = 0; i< count; i++)
			neworder |= ordercache[i]==id;
		if (!neworder)
		{
			ordercache.push_back(id);
			return true;
		}
		return false;
	}

	void TWS_TLServer::openOrder( OrderId orderId, const Contract& contract,
		const Order& order, const OrderState& orderState)
	{
		D1("TWS_TLServer::openOrder");
		// log warnings
		if (orderState.warningText!="")
		{
			v(orderState.warningText);
		}

		int orderPos = findIBOrder(orderId);
		if (orderState.status == "Submitted" || orderState.status == "Filled")
		{
			if (orderPos >= 0)
			{
				if (tlorders[orderPos].ack) return; // already acknowledged
				tlorders[orderPos].ack = true; // mark order as acknowledged
			}
		}
		else
		{
			// ignore other states
			return;
		}
		// count order
		getNextOrderId(order.account);

		// prepare client order and notify client
		TradeLibFast::TLOrder o;
		// see if it's an order with no id (manual/front-end order)
		if (orderPos < 0)
		{
			o.id = saveOrder(orderId,order.account); // this will also mark order as acknowledged
			CString tmp;
			tmp.Format("assigning tlid: %lld to manual ibid: %i",o.id,orderId);
			v(tmp);
		}
		else
		{
			o.id = tlorders[orderPos].id;

			// update account as it might not have been set explicitly
			// account is used for cancelling order
			tlorders[orderPos].account = order.account;
		}
		o.side = (order.action=="BUY");
		o.size = abs(order.totalQuantity) * ((o.side) ? 1 : -1);
		o.symbol = contract.localSymbol;
		o.price = (order.orderType=="LMT") ? order.lmtPrice : 0;
		o.stop = (order.orderType=="STP") ? order.auxPrice : 0;
		o.exchange = contract.exchange;
		o.account = order.account;
		o.security = contract.secType;
		o.currency = contract.currency;
		o.localsymbol = contract.localSymbol;
		if (fillnotifyfullsymbol)
		{
			int idx = getsymbolindex(contract.localSymbol);
			if (idx<0)
			{
				o.localsymbol = contract.localSymbol;
				o.symbol = contract.localSymbol;
			}
			else
			{
				o.symbol = stockticks[idx].sym;
				o.localsymbol = stockticks[idx].sym;
			}

		}

		o.TIF = order.tif;
		std::vector<int> nowtime;
		CString no;
		no.Format("%s received order ack: %s",o.symbol,o.Serialize());
		D(no);
		TLTimeNow(nowtime);
		o.date = nowtime[TLdate];
		o.time = nowtime[TLtime];
		if (contract.secType!="BAG")
		{
			this->SrvGotOrder(o);

			// send deferred fills if any
			pair<deferred_fill_map_type::iterator, deferred_fill_map_type::iterator> range = deferredFills.equal_range(orderId);
			for (deferred_fill_map_type::iterator it = range.first; it != range.second; ++it)
			{
				TLTrade trade = it->second;
				trade.id = o.id; // update order id because it's still not set if it was unacknowledged front end order
				this->SrvGotFill(trade);
			}
			deferredFills.erase(range.first, range.second);
		}
	}

	CString  TWS_TLServer::ib2tlspace(CString ibsym)
	{
		if (ibsym.FindOneOf(" ")==-1)
			return ibsym;
		ibsym.Replace(" ","_");
		return ibsym;
	}

	CString TWS_TLServer::tl2ibspace(CString tlsym)
	{
		if (tlsym.FindOneOf("_")==-1)
			return tlsym;
		tlsym.Replace("_"," ");
		return tlsym;
	}


	int TWS_TLServer::CancelRequest(int64 orderid)
	{
		int orderPos = findTLOrder(orderid);
		if (orderPos < 0)
		{
			CString tmp;
			tmp.Format("unable to cancel, can't find ibid for tlid: %lld",orderid);
			v(tmp);
			return ORDER_NOT_FOUND;
		}

		CString m;
		m.Format("trying to cancel order: %lld",orderid);
		v(m);

		// gets the IB id
		OrderId ibid = iborders[orderPos];

		CString account = tlorders[orderPos].account;
		if (account.IsEmpty())
		{
			// no account specified for order
			// this should only happen if cancelling order that is not yet acknowledged

			// use first valid mlink
			D2("cancelOrder (a.)");
			m_link[this->validlinkids[0]]->cancelOrder(ibid);
		}
		else
		{
			// get client by order account name
			EClient* client = GetOrderSink(account);
			if (client)
			{
				D2("cancelOrder (a.)");
				client->cancelOrder(ibid);
			}
		}
		return OK;
	}

	void TWS_TLServer::winError( const CString &str, int lastError)
	{
		D1("TWS_TLServer::winError: " + str);
		D(str);
	}
	void TWS_TLServer::error(const int id, const int errorCode, const CString errorString)
	{
		D1("TWS_TLServer::error: '" + errorString + "'");
		// for some reason IB sends order cancels as an error rather than
		// as an order update message
		int64 tlid = IB2TLID(id);
		CString msg;
		msg.Format("%s [err:%i] [ibid:%i] [tlid:%lld] [lastcontract:%s] [lastorder:%s]",errorString,errorCode,id,tlid,lastcontract,lastorder);
		if (errorCode==202) 
			this->SrvGotCancel(tlid); // cancels
		else if (IGNOREERRORS) return; // ignore errors during init
		else if (errorCode==2109) return; // ignore 'place outside of market hours' error as we set this on every order
		else D(msg); // print other errors
	}



	//void TWS_TLServer::execDetails( OrderId orderId, const Contract& contract, const Execution& execution) 
	// reqId int The ID of the data request.
	void   TWS_TLServer::execDetails( int reqId,       const Contract& contract, const Execution& execution)
	{ 
		D1("TWS_TLServer::execDetails");
		OrderId orderId = execution.orderId; // dimon: will it fly?

		// convert to a tradelink trade
		TLTrade trade;
		trade.currency = contract.currency;
		trade.account = execution.acctNumber;
		trade.exchange = contract.exchange;
		trade.id = 0;
		trade.xprice = execution.price;
		trade.xsize = execution.shares;
		trade.side = execution.side=="BOT";
		trade.security = contract.secType;
		if (trade.security==CString("OPT"))
		{
			CString m;
			m.Format("%s %s %s %f OPT",contract.symbol,contract.expiry,(contract.right==CString("C")) ? "CALL" : "PUT",contract.strike);
			trade.symbol = m;
		}
		if (trade.security==CString("FOP"))
		{
			CString m;
			m.Format("%s %s %s %f FOP",contract.symbol,contract.expiry,(contract.right==CString("C")) ? "CALL" : "PUT",contract.strike);
			trade.symbol = m;
		}
		else if (fillnotifyfullsymbol)
		{
			int idx = getsymbolindex(contract.localSymbol);
			if (idx<0)
			{
				trade.localsymbol = contract.localSymbol;
				trade.symbol = contract.localSymbol;
			}
			else
			{
				trade.symbol = stockticks[idx].sym;
				trade.localsymbol = stockticks[idx].sym;
			}

		}
		else
		{
			trade.localsymbol = contract.localSymbol;
			trade.symbol = contract.localSymbol;
		}


		// convert date and time
		std::vector<CString> r;
		std::vector<CString> r2;
		gsplit(execution.time," ",r);
		gsplit(r[2],":",r2);
		int sec = atoi(r2[2]);
		trade.xdate = atoi(r[0]);
		trade.xtime = (atoi(r2[0])*10000)+(atoi(r2[1])*100)+sec;
		if (contract.secType!="BAG")
		{
			int pos = findIBOrder(orderId);
			if (pos >= 0)
			{
				trade.id = tlorders[pos].id;
				if (tlorders[pos].ack)
				{
					this->SrvGotFill(trade);
					return;
				}
			}
			// order not found or not acknowledged yet - defer fill notification
			deferredFills.insert(make_pair(orderId, trade));
		}
	}

	void TWS_TLServer::execDetailsEnd( int reqId)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::execDetailsEnd - NOT IMPLEMENTED IN TRADELINK!");
	}


	int TWS_TLServer::getsymbolindex(CString symbol)
	{
		TLSecurity sec = TLSecurity::Deserialize(symbol);
		CString ssym = sec.sym;
		for (uint i = 0; i<stockticks.size(); i++)
		{
			CString clsym = stockticks[i].sym;
			if (clsym==symbol)
				return i;
			TLSecurity csec = TLSecurity::Deserialize(clsym);
			CString cssym = csec.sym;
			if (cssym==symbol)
				return i;
			if (cssym==ssym)
				return i;
		}

		return -1;
	}


	bool TWS_TLServer::hasTicker(CString symbol)
	{
		for (size_t i = 0; i<stockticks.size(); i++)
			if (stockticks[i].sym==symbol) return true;
		return false;
	}

	bool TWS_TLServer::linktest()
	{
		if (validlinkids.size()>0)
			return true;
		D("Operation failed, no TWS instances running.");
		return false;
	}

	CString TWS_TLServer::getmultiplier(TLSecurity sec)
	{
		CString multiplier = CString("1");
		if (sec.sym=="CC") multiplier= "10";
		else if (sec.sym=="DJ") multiplier= "10";
		else if (sec.sym=="ES") multiplier= "25";
		else if (sec.sym=="C") multiplier= "50";
		else if (sec.sym=="KW") multiplier= "50";
		else if (sec.sym=="MW") multiplier= "50";
		else if (sec.sym=="O") multiplier= "50";
		else if (sec.sym=="PL") multiplier= "50";
		else if (sec.sym=="S") multiplier= "50";
		else if (sec.sym=="SI") multiplier= "50";
		else if (sec.sym=="W") multiplier= "50";
		else if (sec.sym=="GC") multiplier= "100";
		else if (sec.sym=="MV") multiplier= "100";
		else if (sec.sym=="ND") multiplier= "100";
		else if (sec.sym=="RR") multiplier= "100";
		else if (sec.sym=="SM") multiplier= "100";
		else if (sec.sym=="LB") multiplier= "110";
		else if (sec.sym=="JO") multiplier= "150";
		else if (sec.sym=="HG") multiplier= "250";
		else if (sec.sym=="KV") multiplier= "250";
		else if (sec.sym=="SP") multiplier= "250";
		else if (sec.sym=="KC") multiplier= "375";
		else if (sec.sym=="LC") multiplier= "400";
		else if (sec.sym=="LH/LE") multiplier= "400";
		else if (sec.sym=="PB") multiplier= "400";
		else if (sec.sym=="HO") multiplier= "420";
		else if (sec.sym=="HU") multiplier= "420";
		else if (sec.sym=="RB") multiplier= "420";
		else if (sec.sym=="CT") multiplier= "500";
		else if (sec.sym=="FC") multiplier= "500";
		else if (sec.sym=="RL") multiplier= "500";
		else if (sec.sym=="YU") multiplier= "500";
		else if (sec.sym=="BO") multiplier= "600";
		else if (sec.sym=="BP") multiplier= "625";
		else if (sec.sym=="AD") multiplier= "1000";
		else if (sec.sym=="CD") multiplier= "1000";
		else if (sec.sym=="CL") multiplier= "1000";
		else if (sec.sym=="DX") multiplier= "1000";
		else if (sec.sym=="FV") multiplier= "1000";
		else if (sec.sym=="MB") multiplier= "1000";
		else if (sec.sym=="TY") multiplier= "1000";
		else if (sec.sym=="US") multiplier= "1000";
		else if (sec.sym=="SB") multiplier= "1120";
		else if (sec.sym=="EU") multiplier= "1250";
		else if (sec.sym=="JY") multiplier= "1250";
		else if (sec.sym=="SF") multiplier= "1250";
		else if (sec.sym=="DA") multiplier= "2000";
		else if (sec.sym=="TU") multiplier= "2000";
		else if (sec.sym=="ED") multiplier= "2500";
		else if (sec.sym=="NG") multiplier= "10000";


		else
			multiplier= "1";

		if (sec.type==1 )   //stock options
			multiplier= "100";
		return multiplier;
	}

	int TWS_TLServer::RegisterStocks(CString clientname)
	{
		//BEGINILDE
		clientname_ = clientname;
		//ENDILDE

		// make sure base function is called
		TLServer_WM::RegisterStocks(clientname);
		// get client id so we can find his symbols
		int cid = this->FindClient(clientname);  
		// make sure at least one TWS is running
		if (!linktest())
			return BROKERSERVER_NOT_FOUND;

		// loop through every stock for this client
		for (unsigned int i = 0; i<stocks[cid].size(); i++)
		{
			// if we already have a subscription to this stock, proceed to next one
			// dimon: this "hasTicker()" fails for me when start fresh and trying to subscribe to CAD.JPY , but if manually skipping to next line it now works! todo: find out why and 2) make sure exchange="IDEALPRO" and type=CASH is set for forex tickers either automatically or better provide exchage as parameter when requesting tickers, like: CAD.JPY(ex:IDEALPRO) <-- which implies CASH in code
			if (hasTicker(stocks[cid][i])) // wtf: hasTicker thinks we already have it, even though we don't!
				continue;
			// get symbol
			TLSecurity sec = TLSecurity::Deserialize(stocks[cid][i]);
			// keep copy of original symbol
			CString lsym = CString(sec.sym);
			// accomodate that ib allows spaces in symbols (tl:_ -> ib: )
			sec.sym = tl2ibspace(sec.sym);

			// otherwise, subscribe to this stock and save it to subscribed list of tickers
			Contract contract;

			contract.multiplier = getmultiplier(sec);	


			contract.localSymbol = sec.sym;
			contract.right = sec.details;
			CString expire;
			expire.Format("%i",sec.date);
			contract.expiry = expire;
			contract.strike = sec.strike;

			if (!sec.hasDest())
				contract.exchange = "SMART";

			// dimon: trying to subscribe to "CAD.JPY" - delete this nasty hack "if" later
			if (sec.sym == "CAD.JPY"){
				contract.exchange = "IDEALPRO";//"SMART";
				//contract.secType = CASH;
				sec.type = TypeFromExchange(contract.exchange);
			}
			// if destination specified use it
			if (sec.hasDest())
				contract.exchange = sec.dest;
			// if we have a destination but no type, try to guess type
			if (!sec.hasType() && sec.hasDest())
				sec.type = TypeFromExchange(contract.exchange);
			// if we still don't have a type, use stock
			if (!sec.hasType())
				sec.type = STK;
			// if we have a stock and it has no destination, use default
			if ((sec.type==STK) && !sec.hasDest())
				contract.exchange = "SMART";
			if (sec.type==CASH)
				contract.currency = truncateat(sec.sym,CString("."));
			else
				contract.currency = _currency;
			contract.secType = TLSecurity::SecurityTypeName(sec.type);
			pcont(&contract);
			CString j;
			CString extra("");
			if (sec.type==OPT)
				extra.Format("%s %s %f",contract.expiry,(contract.right==CString("C")) ? "CALL" : "PUT",contract.strike);

			j.Format("adding this: %s %s %s %s %s",contract.symbol,contract.localSymbol,contract.secType,contract.exchange,extra);
			D(j);//CString("attempting to add symbol ")+CString(sec.sym));

			D2("reqMktData");
			this->m_link[this->validlinkids[0]]->reqMktData((TickerId)stockticks.size(),contract,"",false);
			TLTick k; // create blank tick
			k.sym = stocks[cid][i]; // store long symbol
			stockticks.push_back(k);
			D(CString("Added IB subscription for ")+CString(sec.sym));
		}
		return OK;

	}




	CString TWS_TLServer::truncateat(CString original,CString after)
	{
		CString cpy = CString(original);
		int pidx = cpy.Find(after);
		if (pidx!=-1)
		{
			cpy.Delete(0,cpy.GetLength()-pidx);				
		}
		return cpy;
	}

	CString TWS_TLServer::truncatebefore(CString original,CString before)
	{
		CString cpy = CString(original);
		int pidx = cpy.Find(before);
		if (pidx!=-1)
		{
			cpy.Delete(pidx,cpy.GetLength()-pidx);				
		}
		return cpy;
	}



	void TWS_TLServer::tickPrice( TickerId tickerId, TickType tickType, double price, int canAutoExecute) 
	{ 
		D1("TWS_TLServer::tickPrice");
		if ((tickerId>=0)&&(tickerId<(TickerId)stockticks.size()) && needStock(stockticks[tickerId].sym))
		{
			time_t now;
			time(&now);
			CTime ct(now);
			TLTick k;
			k.date = (ct.GetYear()*10000) + (ct.GetMonth()*100) + ct.GetDay();
			k.time = (ct.GetHour()*10000)+(ct.GetMinute()*100)+ct.GetSecond();
			k.sym = stockticks[tickerId].sym;
			if (tickType==LAST)
			{
				stockticks[tickerId].trade = price;
				k.trade = price;
				k.size = stockticks[tickerId].size;
			}
			else if (tickType==BID)
			{
				stockticks[tickerId].bid = price;
				k.bid = stockticks[tickerId].bid;
				k.bs = stockticks[tickerId].bs;
			}
			else if (tickType==ASK)
			{
				stockticks[tickerId].ask = price;
				k.ask = stockticks[tickerId].ask;
				k.os = stockticks[tickerId].os;
			}

			else return; // not relevant tick info
			if (k.isValid() && needStock(k.sym))
				this->SrvGotTick(k);
		}

	}

	void TWS_TLServer::tickSize( TickerId tickerId, TickType tickType, int size) 
	{ 
		D1("TWS_TLServer::tickSize");
		if ((tickerId>=0)&&(tickerId<(TickerId)stockticks.size()) && needStock(stockticks[tickerId].sym))
		{
			time_t now;
			time(&now);
			CTime ct(now);
			TLTick k;
			k.date = (ct.GetYear()*10000) + (ct.GetMonth()*100) + ct.GetDay();
			k.time = (ct.GetHour()*10000)+(ct.GetMinute()*100) + ct.GetSecond();
			k.sym = stockticks[tickerId].sym;
			TLSecurity sec = TLSecurity::Deserialize(k.sym);
			bool hundrednorm = (sec.type== STK) || (sec.type== NIL );
			if (tickType==LAST_SIZE)
			{
				stockticks[tickerId].size = hundrednorm ? size*100 : size;
				k.trade = stockticks[tickerId].trade;
				k.size = stockticks[tickerId].size;
			}
			else if (tickType==BID_SIZE)
			{
				stockticks[tickerId].bs = size;
				k.bid = stockticks[tickerId].bid;
				k.bs = stockticks[tickerId].bs;
			}
			else if (tickType==ASK_SIZE)
			{
				stockticks[tickerId].os = size;
				k.ask= stockticks[tickerId].ask;
				k.os = stockticks[tickerId].os;
			}
			else return; // not relevant tick info
			if (k.isValid() && needStock(k.sym))
				this->SrvGotTick(k);
		}
	}

	int TWS_TLServer::getposidx(TLPosition pos)
	{
		for (int i = 0; i<(int)poslist.size(); i++)
			if ((poslist[i].Symbol==pos.Symbol) && (poslist[i].Account == pos.Account))
				return i;
		return -1;
	}

	bool TWS_TLServer::havepos(TLPosition pos)
	{
		int idx = getposidx(pos);
		return idx!=-1;
	}

	void TWS_TLServer::updatePortfolio( const Contract& contract, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const CString &accountName) 
	{ 
		D1("TWS_TLServer::updatePortfolio");
		TLPosition pos;
		if (contract.secType==CString("OPT"))
		{
			CString m;
			m.Format("%s %s %s %f OPT",contract.symbol,contract.expiry,(contract.right==CString("C")) ? "CALL" : "PUT",contract.strike);
			pos.Symbol = m;
		}
		if (contract.secType==CString("FOP"))
		{
			CString m;
			m.Format("%s %s %s %f FOP",contract.symbol,contract.expiry,(contract.right==CString("C")) ? "CALL" : "PUT",contract.strike);
			pos.Symbol = m;
		}
		else
			pos.Symbol = contract.localSymbol;
		if (fillnotifyfullsymbol)
		{
			int idx = getsymbolindex(contract.localSymbol);
			if (idx<0)
			{
				pos.Symbol = contract.localSymbol;
			}
			else
			{
				pos.Symbol = stockticks[idx].sym;
			}

		}



		pos.Size = position;
		pos.AvgPrice = marketPrice;
		pos.ClosedPL = realizedPNL;
		pos.Account = accountName;
		int idx = getposidx(pos);
		if (idx<0)
			poslist.push_back(pos);
		else
			poslist[idx] = pos;



	}

	int TWS_TLServer::PositionResponse(CString account, CString clientname)
	{
		for (int i = 0; i<(int)poslist.size(); i++)
			TLSend(POSITIONRESPONSE,poslist[i].Serialize(),clientname);
		return OK;
	}


	//ILDEBEGIN
	void TWS_TLServer::updateMktDepth( TickerId id, int position, int operation, int side, double price, int size)
	{
		D1("TWS_TLServer::updateMktDepth");
		if ((id>=0)&&(id<(TickerId)stockticks.size()) && needStock(stockticks[id].sym))
		{
			time_t now;
			time(&now);
			CTime ct(now);
			TLTick k;
			k.date = (ct.GetYear()*10000) + (ct.GetMonth()*100) + ct.GetDay();
			k.time = (ct.GetHour()*10000) + (ct.GetMinute()*100)+ct.GetSecond();
			k.sym = stockticks[id].sym;

			if (side==1)
			{
				stockticks[id].bid = price;
				k.bid = stockticks[id].bid;
				k.bs = size;
			}
			else if (side==0)
			{
				stockticks[id].ask = price;
				k.ask = stockticks[id].ask;
				k.os = size;
			}
			else return; // not relevant tick info

			//set book depth
			k.depth = position;

			if (k.isValid() && needStock(k.sym))
				this->SrvGotTick(k);
		}
	}

	//the argument "marketmaker" is ignored
	void TWS_TLServer::updateMktDepthL2( TickerId id, int position, CString marketMaker, int operation, 
		int side, double price, int size) 
	{ 
		D1("TWS_TLServer::updateMktDepthL2");
		if ((id>=0)&&(id<(TickerId)stockticks.size()) && needStock(stockticks[id].sym))
		{
			time_t now;
			time(&now);
			CTime ct(now);
			TLTick k;
			k.date = (ct.GetYear()*10000) + (ct.GetMonth()*100) + ct.GetDay();
			k.time = (ct.GetHour()*10000) + (ct.GetMinute()*100)+ct.GetSecond();
			k.sym = stockticks[id].sym;

			if (side==1)
			{
				stockticks[id].bid = price;
				k.bid = stockticks[id].bid;
				k.bs = size;
			}
			else if (side==0)
			{
				stockticks[id].ask = price;
				k.ask = stockticks[id].ask;
				k.os = size;
			}
			else return; // not relevant tick info

			//set book depth
			k.depth = position;

			if (k.isValid() && needStock(k.sym))
				this->SrvGotTick(k);
		}
	}
	//ILDEEND

	//	void TWS_TLServer::tickOptionComputation( TickerId ddeId, TickType field, double impliedVol,
	//		double delta, double modelPrice, double pvDividend) { }

	void TWS_TLServer::tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
		double optPrice, double pvDividend, double gamma, double vega, double theta, double undPrice) 
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::tickOptionComputation - NOT IMPLEMENTED IN TRADELINK!");
	}	

	void TWS_TLServer::openOrderEnd()
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::openOrderEnd - NOT IMPLEMENTED IN TRADELINK!");
	}	

	void TWS_TLServer::accountDownloadEnd(const IBString& accountName)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::accountDownloadEnd - NOT IMPLEMENTED IN TRADELINK!");
	}	


	void TWS_TLServer::tickGeneric(TickerId tickerId, TickType tickType, double value) 
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::tickGeneric - NOT IMPLEMENTED IN TRADELINK!");
	}	

	void TWS_TLServer::tickString(TickerId tickerId, TickType tickType, const CString& value)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::tickString - NOT IMPLEMENTED IN TRADELINK!");
	}	
	void TWS_TLServer::tickEFP(TickerId tickerId, TickType tickType, double basisPoints,
		const CString& formattedBasisPoints, double totalDividends, int holdDays,
		const CString& futureExpiry, double dividendImpact, double dividendsToExpiry)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::tickEFP - NOT IMPLEMENTED IN TRADELINK!");
	}	
	void TWS_TLServer::orderStatus( OrderId orderId, const CString &status, int filled, int remaining, 
		double avgFillPrice, int permId, int parentId, double lastFillPrice,
		int clientId, const CString& whyHeld)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::orderStatus - NOT IMPLEMENTED IN TRADELINK!");
	}	

	void TWS_TLServer::connectionClosed() {
		D1("TWS_TLServer::connectionClosed");
		D("TWS connection closed.");
	}

	void TWS_TLServer::updateAccountTime(const CString &timeStamp)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::updateAccountTime - NOT IMPLEMENTED IN TRADELINK! (timeStamp="+timeStamp+")");
	}	

	void TWS_TLServer::contractDetails( int reqId, const ContractDetails& contractDetails)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::contractDetails - NOT IMPLEMENTED IN TRADELINK!");
	}	

	void TWS_TLServer::bondContractDetails( int reqId, const ContractDetails& contractDetails)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::bondContractDetails - NOT IMPLEMENTED IN TRADELINK!");
	}	

	void TWS_TLServer::contractDetailsEnd( int reqId)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::contractDetailsEnd - NOT IMPLEMENTED IN TRADELINK!");
	}
	//ILDECOMMENTvoid TWS_TLServer::updateMktDepth( TickerId id, int position, int operation, int side, 
	//double price, int size) { }
	void TWS_TLServer::updateNewsBulletin(int msgId, int msgType, const CString& newsMessage, const CString& originExch)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::updateNewsBulletin - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::receiveFA(faDataType pFaDataType, const CString& cxml)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::receiveFA - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::scannerParameters(const CString &xml)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::scannerParameters - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::scannerData(int reqId, int rank, const ContractDetails &contractDetails, const CString &distance,
		const CString &benchmark, const CString &projection, const CString &legsStr) 
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::scannerData - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::scannerDataEnd(int reqId)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::scannerDataEnd - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
		long volume, double wap, int count)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::realtimeBar - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::currentTime(long time) 
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::currentTime - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::fundamentalData(TickerId reqId, const CString& data)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::fundamentalData - NOT IMPLEMENTED IN TRADELINK!");
	}


	//ILDEBEGIN: 
	//*We are requesting market depth for each registered stock
	//*Cancel DOMRequest is not possible by the time being because interface not available in TLServer_WM.h
	int TWS_TLServer::DOMRequest(int depth)
	{ 
		if(clientname_== "")
		{
			D("No ticket has been registered yet.");
			return OK;
		}

		// make sure base function is called
		TLServer_WM::DOMRequest(depth);

		// get client id so we can find his symbols
		int cid = this->FindClient(clientname_); 

		// make sure at least one TWS is running
		if (!linktest())
			return BROKERSERVER_NOT_FOUND;

		// loop through every stock for this client
		for (unsigned int i = 0; i<stocks[cid].size(); i++)
		{
			// if we already have a subscription to this stock, proceed to next one
			if (hasTicker(stocks[cid][i])) 
			{
				// get symbol
				TLSecurity sec = TLSecurity::Deserialize(stocks[cid][i]);
				// keep copy of original symbol
				CString lsym = CString(sec.sym);
				sec.sym = tl2ibspace(sec.sym);

				// otherwise, subscribe to this stock and save it to subscribed list of tickers
				Contract contract;

				// if option, pass options parameters
				if (sec.isCall() || sec.isPut())
				{
					contract.symbol = sec.sym;
					contract.right = sec.details;
					CString expire;
					expire.Format("%i",sec.date);
					contract.expiry = expire;
					contract.strike = sec.strike;
					if (!sec.hasDest())
						contract.exchange = "SMART";
				}
				else // set local symbol to symbol
				{
					contract.localSymbol = sec.sym;
					contract.symbol = sec.sym;
				}

				// if destination specified use it
				if (sec.hasDest())
					contract.exchange = sec.dest;
				// if we have a destination but no type, try to guess type
				if (!sec.hasType() && sec.hasDest())
					sec.type = TypeFromExchange(contract.exchange);
				// if we still don't have a type, use stock
				if (!sec.hasType())
					sec.type = STK;
				// if we have a stock and it has no destination, use default
				if ((sec.type==STK) && !sec.hasDest())
					contract.exchange = "SMART";
				if (sec.type==CASH)
					contract.currency = truncateat(sec.sym,CString("."));
				else
					contract.currency = _currency;
				contract.secType = TLSecurity::SecurityTypeName(sec.type);

				v(CString("attempting to add market depth"));

				TickerId tid = (TickerId)findStockticksTid(stocks[cid][i]);
				if(tid == -1)
					return SYMBOL_NOT_LOADED;

				D2("reqMktDepth");
				this->m_link[this->validlinkids[0]]->reqMktDepth(tid,contract,depth);

				v(CString("Added IB market depth subscription for ")+CString(sec.sym));
			}
		}
		return 0;
	}

	// you'll see IB added few more virtual methods to EWrapper as well as changed parameters for some exixting ones.
	void TWS_TLServer::deltaNeutralValidation(int reqId, const UnderComp& underComp)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::deltaNeutralValidation - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::tickSnapshotEnd( int reqId)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::tickSnapshotEnd - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::marketDataType( TickerId reqId, int marketDataType)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::marketDataType - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::commissionReport( const CommissionReport &commissionReport)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::commissionReport - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::position( const IBString& account, const Contract& contract, int position)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::position - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::positionEnd()
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::positionEnd - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::accountSummary( int reqId, const IBString& account, const IBString& tag, const IBString& value, const IBString& curency)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::accountSummary - NOT IMPLEMENTED IN TRADELINK!");
	}

	void TWS_TLServer::accountSummaryEnd( int reqId)
	{ 
		// dimon: all these with empty {} basically ignored by TrLink?
		D1("TWS_TLServer::accountSummaryEnd - NOT IMPLEMENTED IN TRADELINK!");
	}


	//This methid returns the tickerid for a given symbol or -1
	int TWS_TLServer::findStockticksTid(CString symbol)
	{
		for (uint i=0; i < stockticks.size(); i++) 
		{
			if(stockticks[i].sym.Compare(symbol) == 0)
				return i;
		}
		return -1;
	}
	//ILDEEND
}
