/**************************************************************************************

Copyright © 2004-2012 VMware, Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,  WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations
under the License.

Written by Mark Venguerov 2004-2012

**************************************************************************************/

/**
 * transaction manager structures
 */
#ifndef _TXMGR_H_
#define _TXMGR_H_

#include "utils.h"
#include "pagemgr.h"
#include "session.h"

#define	MAX_PGID		15
#define	PGID_MASK		0x0F
#define	PGID_SHIFT		4

/**
 * special transaction flags for recovery/undo
 */
#define	TXMGR_UNDO		0x0001
#define	TXMGR_ATOMIC	0x0002
#define	TXMGR_RECV		0x0004

#define	DEFAULT_MAX_SS	20	/**< maximum number of snapshots */

class IStoreNotification;

namespace AfyKernel
{

/**
 * snapshot descriptor
 */
class Snapshot
{
	const TXCID		txcid;		/**< snapshot ID */
	Snapshot		*next;		/**< next snapshot in stack */
	ulong			txCnt;		/**< counter of r/o transactions reading this snapshot */
	DLList			data;		/**< snapshot data */
	Snapshot(TXCID txc,Snapshot *nxt) : txcid(txc),next(nxt),txCnt(0) {}
	friend class	TxMgr;
};

/**
 * transaction manager 
 */
class TxMgr
{
	StoreCtx				*const	ctx;
	IStoreNotification		*const	notification;
	Mutex							lock;
	TXID							nextTXID;
	HChain<Session>					activeList;
	ulong							nActive;
	TXCID							lastTXCID;
	Snapshot						*snapshots;
	ulong							nSS;
	ulong							xSS;
public:
					TxMgr(StoreCtx *cx,TXID startTXID=0,IStoreNotification *notItf=NULL,ulong xSnap=DEFAULT_MAX_SS);
	void *operator	new(size_t s,StoreCtx *ctx) {void *p=ctx->malloc(s); if (p==NULL) throw RC_NORESOURCES; return p;}

	RC				startTx(Session *ses,ulong,ulong);
	RC				abortTx(Session *ses,bool fAll);
	RC				commitTx(Session *ses,bool fAll);
	TXCID			assignSnapshot();
	void			releaseSnapshot(TXCID);

	RC				update(class PBlock *pb,PageMgr *,ulong info,const byte *rec=NULL,size_t lrec=0,uint32_t f=0,class PBlock *newp=NULL) const;
	TXID			getLastTXID() {lock.lock(); TXID txid=++nextTXID; lock.unlock(); return txid;}
	void			setTXID(TXID);
	ulong			getNActive() const {return nActive;}
	struct LogActiveTransactions *getActiveTx(LSN&);
	static PageMgr	*getPageMgr(ulong info,StoreCtx *ctx) {return ctx->getPageMgr(PGID(info&PGID_MASK));}
	static bool		isMaster(ulong info) {return (info&PGID_MASK)==PGID_MASTER;}

private:
	RC				start(Session *ses,ulong flags);
	RC				commit(Session *ses,bool fAll=false,bool fFlush=true);
	RC				abort(Session *ses,bool fAll=false);
	void			cleanup(Session *ses,bool fAbort=false);
	friend	class	Session;
	friend	class	MiniTx;
	friend	class	TxSP;
};

/**
 * MiniTx flags
 */
#define	MTX_OK		0x0001		/**< succesful termination */
#define	MTX_FLUSH	0x0002		/**< flush to disk on commit */
#define	MTX_STARTED	0x0004		/**< transaction started */
#define	MTX_SKIP	0x0008		/**< skip commit */
#define	MTX_GLOB	0x0010		/**< global mini-transaction */

/**
 * mini-transaction object
 */
class MiniTx
{
	friend	class	Session;
	friend	class	TxMgr;
	void	*operator new(size_t s) throw() {return NULL;}
	void	operator delete(void *p) {}
private:
	MiniTx					*next;
	Session		*const		ses;
	TXID					oldId;
	TXID					newId;
	TXCID					txcid;
	ulong					state;
	ulong					identity;
	ulong					mtxFlags;
	SubTx					tx;
	LSN						firstLSN;
	LSN						undoNextLSN;
	GrantedLock				*locks;
	TxReuse					reuse;
	RW_LockType				classLocked;
	void					cleanup(TxMgr *txMgr);
public:
	MiniTx(Session *s,ulong mtxf=MTX_FLUSH);
	~MiniTx();
	void ok() {mtxFlags|=MTX_OK;}
};

/**
 * internal transaction or subtransaction holder
 * commits or aborts (sub-)transaction when goes out of scope
 */
class TxSP
{
	void	*operator new(size_t s) throw() {return NULL;}
	void	operator delete(void *p) {}
private:
	Session	*const	ses;
	ulong			flags;
public:
	TxSP(Session *s) : ses(s),flags(0) {assert(s!=NULL);}
	~TxSP();
	RC		start(ulong,ulong txf=0);
	RC		start();
	void	ok() {flags|=MTX_OK;}
	void	resetOk() {flags&=~MTX_OK;}
	bool	isStarted() const {return (flags&MTX_STARTED)!=0;}
};

};

#endif
