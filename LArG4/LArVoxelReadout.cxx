////////////////////////////////////////////////////////////////////////
/// \file  LArVoxelReadout.cxx
/// \brief A Geant4 sensitive detector that accumulates voxel information.
///
/// \version $Id: LArVoxelReadout.cxx,v 1.4 2009/08/12 20:52:14 t962cvs Exp $
/// \author  seligman@nevis.columbia.edu
////////////////////////////////////////////////////////////////////////

#include "LArG4/LArVoxelReadout.h"
#include "LArG4/ParticleListAction.h"
#include "Utilities/DetectorProperties.h"

#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "Geant4/G4HCofThisEvent.hh"
#include "Geant4/G4TouchableHistory.hh"
#include "Geant4/G4Step.hh"
#include "Geant4/G4StepPoint.hh"
#include "Geant4/G4ThreeVector.hh"
#include "Geant4/Randomize.hh"
#include "Geant4/G4Poisson.hh"

#include <iostream>
#include <ctime>
#include <vector>

#include <stdio.h>

namespace larg4 {

  // Constructor.  Note that we force the name of this sensitive
  // detector to be the value expected by LArVoxelListAction.
  LArVoxelReadout::LArVoxelReadout(std::string const& name)
    : G4VSensitiveDetector(name)
  {
    // Initialize values for the electron-cluster calculation.
    fChannelMap.clear();
    fChannels.clear();

    art::ServiceHandle<util::DetectorProperties> detprop;
    fSampleRate       = detprop->SamplingRate();
    fTriggerOffset    = detprop->TriggerOffset();

    sscanf(name.c_str(),"%*19s%1u%*4s%u",&fCstat,&fTPC);
  }

  //---------------------------------------------------------------------------------------
  LArVoxelReadout::~LArVoxelReadout() {}

  //---------------------------------------------------------------------------------------
  // Called at the start of each event.
  void LArVoxelReadout::Initialize(G4HCofThisEvent*)
  {
    fElectronLifetime      = fLarpHandle->ElectronLifetime();
    fArgon39DecayRate      = fLarpHandle->Argon39DecayRate();
    for (int i = 0; i<3; ++i)
      fDriftVelocity[i]    = fLarpHandle->DriftVelocity(fLarpHandle->Efield(i),
							fLarpHandle->Temperature())/1000.;

    fElectronClusterSize   = fLgpHandle->ElectronClusterSize();
    fLongitudinalDiffusion = fLgpHandle->LongitudinalDiffusion();
    fTransverseDiffusion   = fLgpHandle->TransverseDiffusion();
    fDontDriftThem         = fLgpHandle->DisableWireplanes();

    LOG_DEBUG("LArVoxelReadout")  << " e lifetime: "        << fElectronLifetime
				  << "\n Temperature: "     << fLarpHandle->Temperature()
				  << "\n Drift velocity: "  << fDriftVelocity[0]
				  <<" "<<fDriftVelocity[1]<<" "<<fDriftVelocity[2]
				  << "\n Argon 39 Decay Rate: " << fArgon39DecayRate;
  }

  //---------------------------------------------------------------------------------------
  // Called at the end of each event.
  void LArVoxelReadout::EndOfEvent(G4HCofThisEvent*)
  {

    // put in Argon 39 radioactive decays.  
    /// \todo -- make optical flashes to go along with these decays
    
    // get readout window from DetectorProperties service
    // get Argon 39 rate from LArG4 parameter
    // call Drift Ionilzation Electrons
    // negative track number (-39? -3939?)
      
    // break each radioactive decay into two steps to allow hits on two wires.
    // distribute the hits throughout the drift volume 
      
    // consider calling util::LarProperties::Eloss and ElossVar
    // to get more precise range and fluctuate it randomly.  Probably doesn't matter much
      
    if (fArgon39DecayRate > 0){
      const geo::TPCGeo &tpcg = fGeoHandle->TPC(fTPC,fCstat);
      
      
      art::ServiceHandle<util::DetectorProperties> detprop;
      double samplerate  = detprop->SamplingRate(); // in ns/tick
      int readwindow = detprop->ReadOutWindowSize(); // in clock ticks
      double timewindow = samplerate*readwindow;
      double width = tpcg.HalfWidth()*2.0;
      double height = tpcg.HalfHeight()*2.0;
      double length = tpcg.Length();
      double tpcvolume = length*width*height;
      
      double e39 = 0.565;  // Beta energy in MeV
      double expected_decays = tpcvolume*fArgon39DecayRate*timewindow*1E-9;  // the 1E-9 is to convert the ns to seconds
      double dx = 0.565/2.12; // range assuming MIP (use Eloss to get a better range)
      double d2 = dx*0.5;

      // TPC global position
      
      double xyz1[3] = {0.,0.,0.};
      double xyz2[3] = {0.,0.,0.};
      tpcg.LocalToWorld(xyz1,xyz2);  // gives a point in the middle
      xyz2[0] -= width * 0.5;
      xyz2[1] -= height * 0.5;
      xyz2[2] -= length * 0.5;
      
      int ndecays = G4Poisson(expected_decays);
      for (int i=0;i<ndecays;++i){
	// randomize the simulation time and position
	
	double xyz3[3]={0.,0.,0.};
	
	bool scc = false;
	xyz3[0] = G4UniformRand();
	xyz3[1] = G4UniformRand();
	xyz3[2] = G4UniformRand();
	
	xyz3[0] = (xyz3[0]*width  + xyz2[0])*cm;
	xyz3[1] = (xyz3[1]*height + xyz2[1])*cm;
	xyz3[2] = (xyz3[2]*length + xyz2[2])*cm;
	
	G4ThreeVector midpoint1(xyz3[0],xyz3[1],xyz3[2]);
	G4ThreeVector midpoint2;
	
	scc = false;
	for (int itr=0;itr<20; ++itr){  // fixed number of tries to make sure we stay in the TPC for the second endpoint.
	
	  G4ThreeVector decpath(1.0,0.0,0.0);  
	  decpath.setMag(d2*cm);  // displacement between midpoints is half the total length
	  double angle = G4UniformRand()*TMath::Pi()*2.0;
	  decpath.setPhi(angle);
	  double ct=2.0*(G4UniformRand()-0.5);
	  if (ct<-1.0) ct=-1.0;
	  if (ct>1.0) ct=1.0;
	  angle = TMath::ACos(ct);
	  decpath.setTheta(angle);
	  midpoint2 = midpoint1 + decpath;
	  if (  (midpoint2.x() > xyz2[0]*cm && midpoint2.x() < (xyz2[0]+width)*cm) &&
		(midpoint2.y() > xyz2[1]*cm && midpoint2.y() < (xyz2[1]+height)*cm) &&
		(midpoint2.z() > xyz2[2]*cm && midpoint2.z() < (xyz2[2]+length)*cm) ){
	    scc = true;
	    break;
	  }
	}

	if (scc){
	  // Have to make a G4Step in order to use the IonizationAndScintillation service

	  // make a G4Step to be passed to the IonizationAndScintillation instance to reset it
	  G4ThreeVector pretv(0., 0., 0.);
	  G4ThreeVector posttv(0., 0., dx);
	  G4StepPoint preStep  = G4StepPoint();
	  G4StepPoint postStep = G4StepPoint();
	  preStep.SetPosition(pretv);
	  postStep.SetPosition(posttv);

	  // we only use the magnitude of the step and the energy
	  // deposited in the service, so only set those.
	  /// \todo Make sure the decays are handled properly if using the NEST version of ISCalculation.
	  G4Step step = G4Step();
	  step.SetTotalEnergyDeposit(0.5*e39);
	  step.SetPreStepPoint(&preStep);
	  step.SetPostStepPoint(&postStep);
	  larg4::IonizationAndScintillation::Instance()->Reset(&step);

	  DriftIonizationElectrons(midpoint1,0.0,-(UINT_MAX - 10),firstrad,readwindow);
	  DriftIonizationElectrons(midpoint2,0.0,-(UINT_MAX - 10),subsequentrad,readwindow);
	}
      }
    }
  }

  //---------------------------------------------------------------------------------------
  void LArVoxelReadout::clear()
  {
  }

  //---------------------------------------------------------------------------------------
  void LArVoxelReadout::MakeSimChannelVector()
  {
    // iterate over the map and fill the vector of channels
    std::map<uint32_t, sim::SimChannel>::const_iterator itr = fChannelMap.begin();
    while( itr != fChannelMap.end() ){

      fChannels.push_back((itr->second));
      itr++;
    }

    return;
  }

  //--------------------------------------------------------------------------------------
  void LArVoxelReadout::ClearSimChannelVector()
  {
    fChannelMap.clear();
    fChannels.clear();
  }

  //---------------------------------------------------------------------------------------
  // Called for each step.
  G4bool LArVoxelReadout::ProcessHits( G4Step* step, G4TouchableHistory* )
  {
    // All work done for the "parallel world" "box of voxels" in
    // LArVoxelReadoutGeometry makes this a fairly simple routine.
    // First, the usual check for non-zero energy:

    // Only process the hit if the step is inside the active volume and
    // it has deposited energy.  The hit being inside the active volume
    // is virtually sure to happen because the LArVoxelReadoutGeometry
    // that this class makes use of only has voxels for inside the TPC.

    // The step can be no bigger than the size of the voxel,
    // because of the geometry set up in LArVoxelGeometry and the
    // transportation set up in PhysicsList.  Find the mid-point
    // of the step.

    if ( step->GetTotalEnergyDeposit() > 0 ){
      
      // Make sure we have the IonizationAndScintillation singleton
      // reset to this step
      larg4::IonizationAndScintillation::Instance()->Reset(step);

      if( !fDontDriftThem ){

	G4ThreeVector midPoint = 0.5*( step->GetPreStepPoint()->GetPosition()
				       + step->GetPostStepPoint()->GetPosition() );
	double g4time = step->GetPreStepPoint()->GetGlobalTime();
	
	// Find the Geant4 track ID for the particle responsible for depositing the
	// energy.  if we are only storing primary EM shower particles, and this energy
	// is from a secondary etc EM shower particle, the ID returned is the primary
	const int trackID = ParticleListAction::GetCurrentTrackID();
	
	// Note that if there is no particle ID for this energy deposit, the
	// trackID will be sim::NoParticleId.
	DriftIonizationElectrons(midPoint, g4time, trackID);
      } // end we are drifting
    } // end there is non-zero energy deposition

    return true;
  }

  //----------------------------------------------------------------------------
  // energy is passed in with units of MeV, dx has units of cm
  void LArVoxelReadout::DriftIonizationElectrons(G4ThreeVector stepMidPoint,
						 const double simTime,
						 int trackID,
						 Radio_t radiological,
						 unsigned int tickmax)
  {


    // This routine gets called frequently, once per every particle
    // traveling through every voxel. Use whatever tricks we can to
    // increase its execution speed.

    static double LifetimeCorr_const = -1000. * fElectronLifetime;
    static double LDiff_const        = std::sqrt(2.*fLongitudinalDiffusion);
    static double TDiff_const        = std::sqrt(2.*fTransverseDiffusion);
    static double RecipDriftVel[3]   = {1./fDriftVelocity[0], 
					1./fDriftVelocity[1], 
					1./fDriftVelocity[2]};

    static int radiologicaltdcoffset=0;  // static so remembers for subsequent calls for radiological sim

    // Map of electrons to store - catalogued by map[channel][tdc]
    static std::map<uint32_t, std::map<unsigned int,double> >  ElectronsToStore;
    static std::map<uint32_t, std::map<unsigned int,double> >  EnergyToStore;

    static double xyz1[3] = {0.};

    double xyz[3] = {stepMidPoint.x() / cm,
		     stepMidPoint.y() / cm,
		     stepMidPoint.z() / cm};

    // Already know which TPC we're in because each LArVoxelReadout instance is
    // associated with just one

    try{
      const geo::TPCGeo &tpcg = fGeoHandle->TPC(fTPC,fCstat);

      // X drift distance - the drift direction can be either in
      // the positive or negative direction, so use std::abs
      double XDrift = std::abs(stepMidPoint.x()/cm - tpcg.PlaneLocation(0)[0]);

      if(XDrift < 0.) return;

      // Drift time (nano-sec)
      double TDrift             = XDrift * RecipDriftVel[0];
      if (tpcg.Nplanes() == 2){// special case for ArgoNeuT (plane 0 is the second wire plane)
	TDrift = ((XDrift - tpcg.PlanePitch(0,1)) * RecipDriftVel[0] 
		  + tpcg.PlanePitch(0,1) * RecipDriftVel[1]);
      }
	  
      double lifetimecorrection = TMath::Exp(TDrift / LifetimeCorr_const);
      double nElectrons         = larg4::IonizationAndScintillation::Instance()->NumberIonizationElectrons();
             nElectrons        *= lifetimecorrection;
      double energy             = larg4::IonizationAndScintillation::Instance()->EnergyDeposit();

      // Longitudinal & transverse diffusion sigma (cm)
      double SqrtT    = std::sqrt(TDrift);
      double LDiffSig = SqrtT * LDiff_const;
      double TDiffSig = SqrtT * TDiff_const;
      int nClus       = 1 + (int)(nElectrons / fElectronClusterSize);

      // Compute arrays of values as quickly as possible.
      std::vector< double > XDiff(nClus);
      std::vector< double > YDiff(nClus);
      std::vector< double > ZDiff(nClus);
      std::vector< double > nElDiff(nClus, fElectronClusterSize);
      std::vector< double > nEnDiff(nClus, fElectronClusterSize);

      // Drift the last cluster with smaller size
      nElDiff[nClus-1] = nElectrons - (nClus-1)*fElectronClusterSize;

      for(size_t xx = 0; xx < nElDiff.size(); ++xx){
	if(nElectrons > 0) nEnDiff[xx] = energy/nElectrons*nElDiff[xx];
	else               nEnDiff[xx] = 0.;
      }

      // Smear drift times by x position and drift time
      G4RandGauss::shootArray( nClus, &XDiff[0], 0., LDiffSig);

      // Smear the Y,Z position by the transverse diffusion
      G4RandGauss::shootArray( nClus, &YDiff[0], stepMidPoint.y()/cm,TDiffSig);
      G4RandGauss::shootArray( nClus, &ZDiff[0], stepMidPoint.z()/cm,TDiffSig);

      // make a collection of electrons for each plane
      for(size_t p = 0; p < tpcg.Nplanes(); ++p){

	double Plane0Pitch = tpcg.Plane0Pitch(p);
	
	// "-" sign is because Plane0Pitch output is positive. Andrzej
	xyz1[0] = tpcg.PlaneLocation(0)[0] - Plane0Pitch;
	
	// Drift nClus electron clusters to the induction plane
	for(int k = 0; k < nClus; ++k){
	  // Correct drift time for longitudinal diffusion and plane
	  double TDiff = TDrift + XDiff[k] * RecipDriftVel[0];
	  // Take into account different Efields between planes
	  // Also take into account special case for ArgoNeuT where Nplanes = 2.
	  for (size_t ip = 0; ip<p; ++ip){
	    TDiff += tpcg.PlanePitch(ip,ip+1) * RecipDriftVel[tpcg.Nplanes()==3?ip+1:ip+2];
	  }
	  xyz1[1] = YDiff[k];
	  xyz1[2] = ZDiff[k];
	  
	  /// \todo think about effects of drift between planes
	  
	  // grab the nearest channel to the xyz position
	  try{
	    uint32_t channel = fGeoHandle->NearestChannel(xyz1, p, fTPC, fCstat);
	    
	    /// \todo check on what happens if we allow the tdc value to be
	    /// \todo beyond the end of the expected number of ticks
	    // Add potential decay/capture/etc delay effect, simTime.
	    unsigned int tdc = (unsigned int)((TDiff + simTime) / fSampleRate + fTriggerOffset);

	    // on the first time we decide a tdc for a radiological, throw a random time for it.
	    // save it for other clusters, planes, and second (or subsequent) calls to this function

            if (radiological != notradiological){
	      if (p == 0 && k == 0){
		double tmptdc = ((double) tickmax) * G4UniformRand();
		if (radiological == firstrad) radiologicaltdcoffset = tmptdc - tdc;
	      }
	      if ( (int) tdc >= -radiologicaltdcoffset)  {
		tdc += radiologicaltdcoffset;
	      }
	      // skip the radiological deposition if it results in a negative tdc.
	      // should only happen due to diffusion or wire plane spacing.
	      else 
		continue;  
		
	    }
	    
	    // Add electrons produced by each cluster to the map
	    EnergyToStore[channel][tdc]    += nEnDiff[k];
	    ElectronsToStore[channel][tdc] += nElDiff[k];
	  }
	  catch(cet::exception &e){
	    mf::LogWarning("LArVoxelReadout") << "unable to drift electrons from point ("
					      << xyz[0] << "," << xyz[1] << "," << xyz[2]
					      << ") with exception " << e;
	  }
	} // end loop over clusters
      } // end loop over planes
      
      // Now store them in SimChannels
      
      // check if the current channel is already in the map, otherwise add it
      for(auto const& itread : ElectronsToStore){
	
	uint32_t channel = itread.first;
	std::map<uint32_t, sim::SimChannel>::iterator itchannelmap = fChannelMap.find(channel);
	
	if( itchannelmap != fChannelMap.end() ){
	  for(auto const& itreadinner : itread.second)
	    itchannelmap->second.AddIonizationElectrons(trackID,
							itreadinner.first,
							itreadinner.second,
							xyz,
							EnergyToStore[channel][itreadinner.first]);
	}
	else{
	  sim::SimChannel sc(channel);
	  for(auto const& itreadinner : itread.second)
	    sc.AddIonizationElectrons(trackID,
				      itreadinner.first,
				      itreadinner.second,
				      xyz,
				      EnergyToStore[channel][itreadinner.first]);
	  
	  fChannelMap[channel] = sc;
	}
      }
      ElectronsToStore.clear();
      EnergyToStore.clear();
      
    } // end try intended to catch points where TPC can't be found
    catch(cet::exception &e){
      mf::LogWarning("LArVoxelReadout") << "step cannot be found in a TPC\n"
					<< e;
    }
    
    return;
  }

  //---------------------------------------------------------------------------------------
  // Never used but still have to be defined for G4
  void LArVoxelReadout::DrawAll()  {}
  void LArVoxelReadout::PrintAll() {}

} // namespace larg4