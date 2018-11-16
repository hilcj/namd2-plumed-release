/**
***  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000 by
***  The Board of Trustees of the University of Illinois.
***  All rights reserved.
**/

/*
   HomePatch is the key distributed source/sink of Atom data
   including positions, velocities and forces applied
*/

#ifndef HOMEPATCH_H
#define HOMEPATCH_H

#include "charm++.h"

#include "NamdTypes.h"
#include "Patch.h"
#include "PatchMap.h"

#include "MigrateAtomsMsg.h"
#include "main.h"
#include "common.h"
#include "Migration.h"
#include "Settle.h"

#include <string>
#include <map>

//
// DJH: NAMDLite array buffers are memory aligned for vector instructions.
//
//#include "nl_Array.h"
//

class RegisterProxyMsg;
class UnregisterProxyMsg;
class ProxyResultVarsizeMsg;
class ProxyResultMsg;
class ProxyCombinedResultRawMsg;
class Sequencer;
class SubmitReduction;
class ProxyGBISP1ResultMsg;
class ProxyGBISP2ResultMsg;
class CheckpointAtomsMsg;
class ExchangeAtomsMsg;

class ProxyNodeAwareSpanningTreeMsg;

class ComputeQMMgr;

//
// DJH: Array buffers defined here for storing data from FullAtomList and
// also forces into SOA (structure of arrays) layout. Also declare methods
// for copying from AOS to SOA and back again.
//
// We will also remove derived constants from FullAtom (e.g. recipMass).
// The idea is reduce the messaging footprint as much as possible.
// Recalculate constants after atom migration.
//
//struct PatchDataSOA {
//
//namdlite::Array<float> gaussrand; // fill with Gaussian random numbers
//
//namdlite::Array<float> mass;
//namdlite::Array<float> recipMass; // derived from mass
//namdlite::Array<float> langevinParam;
//namdlite::Array<float> langScalVelBBK2;  // derived from langevinParam
//namdlite::Array<float> langScalRandBBK2; // from langevinParam and recipMass
//
//namdlite::Array<double> vel_x;  // Jim recommends double precision velocity
//namdlite::Array<double> vel_y;
//namdlite::Array<double> vel_z;
//namdlite::Array<double> pos_x;
//namdlite::Array<double> pos_y;
//namdlite::Array<double> pos_z;
//namdlite::Array<double> f_normal_x;
//namdlite::Array<double> f_normal_y;
//namdlite::Array<double> f_normal_z;
//namdlite::Array<double> f_nbond_x;
//namdlite::Array<double> f_nbond_y;
//namdlite::Array<double> f_nbond_z;
//namdlite::Array<double> f_slow_x;
//namdlite::Array<double> f_slow_y;
//namdlite::Array<double> f_slow_z;
//};

class HomePatch : public Patch {
  friend class PatchMgr;
  friend class Sequencer;
  friend class ComputeGlobal;

private: 
  // for PatchMgr to use only
  HomePatch(PatchID, FullAtomList&);

  void reinitAtoms(FullAtomList&);
  ScaledPosition min, max, center;
  BigReal aAwayDist, bAwayDist, cAwayDist;

  Bool doAtomUpdate;  // atom changes other than migration

  //Note: If new proxies are added to this HomePatch
  // after load balancing, and it is not the immediate step
  // after atom migration (where ProxyAllMsg will be sent), 
  // then the CompAtomExt list has to be resent with the 
  // ProxyDataMsg (the normal proxy msg when atoms don't 
  // migrate), otherwise, program will crash without such 
  // information when doing force calculations --Chao Mei
  Bool isNewProxyAdded;
  int numGBISP1Arrived, numGBISP2Arrived, numGBISP3Arrived;
  bool phase1BoxClosedCalled;
  bool phase2BoxClosedCalled;
  bool phase3BoxClosedCalled;

public:
  ~HomePatch();

  // Message from ProxyPatch (via ProxyMgr) which registers its existence
  void registerProxy(RegisterProxyMsg *);
  // opposite of above
  void unregisterProxy(UnregisterProxyMsg *);

  // ProxyPatch sends Forces back to here (via ProxyMgr)  
  void receiveResults(ProxyResultVarsizeMsg *msg);
  void receiveResults(ProxyResultMsg *msg);     
  //gbis receiving results from intermediate phases
  void receiveResult(ProxyGBISP1ResultMsg *msg);//after P1
  void receiveResult(ProxyGBISP2ResultMsg *msg);//after P2
  
  //direct function calls, not as entry methods
  void receiveResults(ProxyCombinedResultRawMsg *msg);

  // AtomMigration messages passes from neighbor HomePatches to here.
  void depositMigration(MigrateAtomsMsg *);

  // Bind a Sequencer to this HomePatch
  void useSequencer(Sequencer *sequencerPtr);
  // start simulation over this Patch of atoms
  void runSequencer(void);
  
  //--------------------------------------------------------------------
  // methods for Sequencer to use
  //

  // Signal HomePatch that positions stored are to be now to be used
  void positionsReady(int doMigration=0);
  int marginViolations;

  // methods to implement integration
  void saveForce(const int ftag = Results::normal);
  void addForceToMomentum(
      FullAtom       * __restrict atom_arr,
      const Force    * __restrict force_arr,
      const BigReal    dt,
      int              num_atoms
      )
#if !defined(WIN32) && !defined(WIN64)
    __attribute__((__noinline__))
#endif
    ;
  void addForceToMomentum3(
      FullAtom       * __restrict atom_arr,
      const Force    * __restrict force_arr1,
      const Force    * __restrict force_arr2,
      const Force    * __restrict force_arr3,
      const BigReal    dt1,
      const BigReal    dt2,
      const BigReal    dt3,
      int              num_atoms
      ) 
#if !defined(WIN32) && !defined(WIN64)
    __attribute__((__noinline__))
#endif
    ;
  void addVelocityToPosition(
      FullAtom       * __restrict atom_arr,
      const BigReal    dt,
      int              num_atoms
      ) 
#if !defined(WIN32) && !defined(WIN64)
    __attribute__((__noinline__))
#endif
    ;

  // impose hard wall constraint on Drude bond length
  int hardWallDrude(const BigReal, Tensor *virial, SubmitReduction *);

  // methods for rigidBonds
  struct RattleList {
    int ig;
    int icnt;
  };

  std::vector<int> settleList;
  std::vector<RattleList> rattleList;
  std::vector<RattleParam> rattleParam;
  std::vector<int> noconstList;

  bool rattleListValid;

  // Array to store new positions and velocities. Allocated in "buildRattleList" to size numAtoms
  std::vector<Vector> velNew;
  std::vector<Vector> posNew;

  void addRattleForce(const BigReal invdt, Tensor& wc);

  void buildRattleList();
  int rattle1old(const BigReal, Tensor *virial, SubmitReduction *);
  int rattle1(const BigReal, Tensor *virial, SubmitReduction *);
  void rattle2(const BigReal, Tensor *virial);
  void minimize_rattle2(const BigReal, Tensor *virial, bool forces=false);

  // methods for mollified impluse (MOLLY)
  void mollyAverage();
  void mollyMollify(Tensor *virial);
//  Bool average(Vector qtilde[],const Vector q[],BigReal lambda[],const int n,const int m, const BigReal imass[], const BigReal length2[], const int ial[], const int ilb[], const Vector qji[], const BigReal tolf, const int ntrial);
//  void mollify(Vector qtilde[],const Vector q0[],const BigReal lambda[], Vector force[],const int n, const int m, const BigReal imass[],const int ial[],const int ibl[],const Vector refab[]); 
  
  // BEGIN LA
  void loweAndersenVelocities();
  void loweAndersenFinish();
  // END LA

  void setGBISIntrinsicRadii();
  void gbisComputeAfterP1();//calculate bornRad
  void gbisComputeAfterP2();//calculate dHdrPrefix or self energies
  void gbisP2Ready();
  void gbisP3Ready();

  //LCPO
  void setLcpoType();

  // methods for CONTRA, etc
  void checkpoint(void);
  void revert(void);

  void exchangeCheckpoint(int scriptTask, int &bpc);
  void recvCheckpointReq(int task, const char *key, int replica, int pe);
  void recvCheckpointLoad(CheckpointAtomsMsg *msg);
  void recvCheckpointStore(CheckpointAtomsMsg *msg);
  void recvCheckpointAck();
  int checkpoint_task;
  struct checkpoint_t {
    Lattice lattice;
    int berendsenPressure_count;
    int numAtoms;
    ResizeArray<FullAtom> atoms;
  };
  std::map<std::string,checkpoint_t*> checkpoints;

  // replica exchange
  void exchangeAtoms(int scriptTask);
  void recvExchangeReq(int req);
  void recvExchangeMsg(ExchangeAtomsMsg *msg);
  int exchange_dst;
  int exchange_src;
  int exchange_req;
  ExchangeAtomsMsg *exchange_msg;

  // methods for QM (ExtForces replacement)
  void replaceForces(ExtForce *f);

  void qmSwapAtoms();
  
  // load-balancing trigger
  void submitLoadStats(int timestep);

  // for ComputeHomePatches
  FullAtomList &getAtomList() { return (atom); }

#ifdef NODEAWARE_PROXY_SPANNINGTREE
  // build spanning tree for proxy nodes
  void buildNodeAwareSpanningTree(void);
  void setupChildrenFromProxySpanningTree();
#else
    // build spanning tree for proxy nodes
  void buildSpanningTree(void);
#endif

  void sendNodeAwareSpanningTree();
  void recvNodeAwareSpanningTree(ProxyNodeAwareSpanningTreeMsg *msg);

  void sendSpanningTree();
  void recvSpanningTree(int *t, int n);


  void sendProxies();

#if USE_TOPOMAP 
  int findSubroots(int dim, int* subroots, int psize, int* pidscopy);
#endif

  LDObjHandle ldObjHandle;
protected:
  virtual void boxClosed(int);

  // Internal Atom Migration methods and data
  void doPairlistCheck();
  void doGroupSizeCheck();
  void doMarginCheck();
  void doAtomMigration();
  int inMigration;
  int numMlBuf;
  MigrateAtomsMsg *msgbuf[PatchMap::MaxOneAway];
  
private:
  // Store of Atom-wise variables
  FullAtomList  atom;
  ForceList f_saved[Results::maxNumForces];
  ExtForce *replacementForces;

  CudaAtomList cudaAtomList;

  //
  // DJH: SOA data structure declared here.
  //
  //PatchDataSOA patchDataSOA;
  //
  // Copy fields from FullAtom into SOA form.
  //void copy_atoms_to_SOA();
  //
  // Copy forces into SOA form.
  //void copy_forces_to_SOA();
  //
  // Calculate derived constants after atom migration.
  //void calculate_derived_SOA();
  //
  // Copy the updated quantities, e.g., positions and velocities, from SOA
  // back to AOS form.
  //void copy_updates_to_AOS();
  //

  // DMK - Atom Separation (water vs. non-water)
  #if NAMD_SeparateWaters != 0
    FullAtomList tempAtom;  // A temporary array used to sort waters
                            //   from non-waters in the atom array
    void separateAtoms();   // Function to separate the atoms currently in atoms.
    void mergeAtomList(FullAtomList &al);  // Function to combine and separate
                                           //   the atoms in al with atoms.
  #endif


  // checkpointed state
  FullAtomList  checkpoint_atom;
  Lattice  checkpoint_lattice;

  // DMK - Atom Separation (water vs. non-water)
  #if NAMD_SeparateWaters != 0
    int checkpoint_numWaterAtoms;
  #endif


  // checkPairlist data
  CompAtomList doPairlistCheck_positions;
  Lattice doPairlistCheck_lattice;
  BigReal doPairlistCheck_newTolerance;

  // MOLLY data
  ResizeArray<BigReal> molly_lambda;
  
  // List of Proxies
  NodeIDList proxy;
  
  Sequencer  *sequencer;

  // Needed for initialization
  int patchMapRead;
  void readPatchMap();

  // Atom Migration internals
  int allMigrationIn;
  int migrationSuspended;
  int patchMigrationCounter;
  int numNeighbors;
  MigrationInfo realInfo[PatchMap::MaxOneAway];
  MigrationInfo *mInfo[3][3][3];

#ifdef NODEAWARE_PROXY_SPANNINGTREE
  //the whole spanning tree for all the proxies this home patch has
  proxyTreeNodeList ptnTree;
  //the immediate children (recording pe ids) containing two parts: 
  //one part of them all belong to the physical node this home patch
  // resides on; the other part of pes belong to all external nodes.
  /* Moved to Patch.h */ 
  //int *children;
  //int numChild;
#else
  NodeIDList tree;              // the whole tree
  int *child;	// spanning tree of proxies - immediate children
  int nChild;
#endif

  // Cached settle1 parameters
  int settle_initialized;
  BigReal settle_mOrmT; BigReal settle_mHrmT; BigReal settle_ra;
  BigReal settle_rb; BigReal settle_rc; BigReal settle_rra;

  /**
   * Redistribute all lonepair forces (of any kind). This may include a direct
   * correction to the virial.
   */
  void redistrib_lonepair_forces(const int, Tensor *);

  // PLF -- for TIP4P
  //void redistrib_tip4p_force(Vector&, Vector&, Vector&, Vector&, int, Tensor*);
  void redistrib_tip4p_forces(const int, Tensor*);
  void tip4_omrepos(Vector*, Vector*, Vector*, BigReal);
  void init_tip4();

  // Drude SWM4
  void redistrib_swm4_forces(const int, Tensor*);
  void swm4_omrepos(Vector*, Vector*, Vector*, BigReal);
  void init_swm4();

  /**
   * Reposition lonepair i in a colinear fashion relative to its hosts j and k
   * and according to a fixed distance and scaled vector magnitude between the
   * two hosts.
   */
  void reposition_colinear_lonepair(
      Vector& ri, const Vector& rj, const Vector& rk, Real distance,
      Real scale);

  /**
   * Reposition a lonepair i relative to its hosts j, k, and l according to a
   * given distance, angle, and dihedral formed with the three hosts.
   */
  void reposition_relative_lonepair(
      Vector& ri, const Vector& rj, const Vector& rk, const Vector& rl,
      Real distance, Real angle, Real dihedral);

  /**
   * Reposition all lonepairs (of any kind).
   */
  void reposition_all_lonepairs(void);

  /**
   * Redistribute the force on a colinear lonepair onto its hosts.
   */
  void redistrib_colinear_lp_force(
       Vector& fi, Vector& fj, Vector& fk,
       const Vector& ri, const Vector& rj, const Vector& rk,
       Real distance, Real scale);

  /**
   * Redistribute the force on a relative lonepair onto its hosts.
   */
  void redistrib_relative_lp_force(
      Vector& fi, Vector& fj, Vector& fk, Vector& fl,
      const Vector& ri, const Vector& rj, const Vector& rk, const Vector& rl,
      Tensor *virial, int midpt);

  /**
   * Redistribute the force on a water (TIP4P, SWM4) lonepair onto its hosts.
   * This is similar to redistrib_relative_lp_force but specialized for the
   * bisector case.
   */
  void redistrib_lp_water_force(
      Vector& f_ox, Vector& f_h1, Vector& f_h2, Vector& f_lp,
      const Vector& p_ox, const Vector& p_h1, const Vector& p_h2,
      const Vector& p_lp, Tensor *virial);

  BigReal r_om, r_ohc;
  void write_tip4_props(void);

  int isProxyChanged;

#if CMK_PERSISTENT_COMM
  PersistentHandle *localphs;
  int nphs;
#endif
};

#endif

