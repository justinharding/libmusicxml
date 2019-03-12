/*
  MusicXML Library
  Copyright (C) Grame 2006-2013

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr
*/

#include "msrMutualDependencies.h"

#include "setTraceOptionsIfDesired.h"
#ifdef TRACE_OPTIONS
  #include "traceOptions.h"
#endif

#include "musicXMLOptions.h"
#include "msrOptions.h"


using namespace std;

namespace MusicXML2 
{

//______________________________________________________________________________
int msrStaff::gStaffMaxRegularVoices = 4;

S_msrStaff msrStaff::create (
  int          inputLineNumber,
  msrStaffKind staffKind,
  int          staffNumber,
  S_msrPart    staffPartUplink)
{
  msrStaff* o =
    new msrStaff (
      inputLineNumber,
      staffKind,
      staffNumber,
      staffPartUplink);
  assert(o!=0);

  return o;
}

msrStaff::msrStaff (
  int          inputLineNumber,
  msrStaffKind staffKind,
  int          staffNumber,
  S_msrPart    staffPartUplink)
    : msrElement (inputLineNumber)
{
  // sanity check
  msrAssert(
    staffPartUplink != nullptr,
    "staffPartUplink is null");

  // set staff part uplink
  fStaffPartUplink =
    staffPartUplink;

  // set staff kind and number
  fStaffKind   = staffKind;
  fStaffNumber = staffNumber;

  // do other initializations
  initializeStaff ();
}

void msrStaff::initializeStaff ()
{
  fStaffRegularVoicesCounter = 0;

  // set staff name
  switch (fStaffKind) {
    case msrStaff::kStaffRegular:
      fStaffName =
        fStaffPartUplink->getPartMsrName () +
        "_Staff_" +
        int2EnglishWord (fStaffNumber);
      break;
      
    case msrStaff::kStaffTablature:
        fStaffPartUplink->getPartMsrName () +
        "_Tablature_" +
        int2EnglishWord (fStaffNumber);
      break;
      
    case msrStaff::kStaffHarmony:
      fStaffName =
        fStaffPartUplink->getPartMsrName () +
        "_HARMONY_Staff";
      break;
      
    case msrStaff::kStaffFiguredBass:
      fStaffName =
        fStaffPartUplink->getPartMsrName () +
        "_FIGURED_BASS_Staff";
      break;
      
    case msrStaff::kStaffDrum:
      fStaffName =
        fStaffPartUplink->getPartMsrName () +
        "_DRUM_Staff";
      break;
      
    case msrStaff::kStaffRythmic:
      fStaffName =
        fStaffPartUplink->getPartMsrName () +
        "_RYTHMIC_Staff";
      break;
  } // switch

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Initializing staff \"" << fStaffName <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  gIndenter++;
  
  // check the staff number
  switch (fStaffKind) {
    case msrStaff::kStaffRegular:
      // the staff number should not be negative
      if (fStaffNumber < 0) {
        stringstream s;
    
        s <<
          "regular staff number " << fStaffNumber <<
          " is not positive";
          
        msrAssert (
          false,
          s.str ());
      }
      break;
      
    case msrStaff::kStaffTablature:
      break;
      
    case msrStaff::kStaffHarmony:
      break;
      
    case msrStaff::kStaffFiguredBass:
      if (fStaffNumber != K_PART_FIGURED_BASS_STAFF_NUMBER) {
        stringstream s;
    
        s <<
          "figured bass staff number " << fStaffNumber <<
          " is not equal to " << K_PART_FIGURED_BASS_STAFF_NUMBER;
          
        msrInternalError (
          gGeneralOptions->fInputSourceName,
          fInputLineNumber,
          __FILE__, __LINE__,
          s.str ());
      }
      break;
      
    case msrStaff::kStaffDrum:
      break;
      
    case msrStaff::kStaffRythmic:
      break;
  } // switch

  // get the initial staff details from the part if any
  S_msrStaffDetails
    partStaffDetails =
      fStaffPartUplink->
        getCurrentPartStaffDetails ();

  if (partStaffDetails) {
    // append it to the staff
    appendStaffDetailsToStaff (partStaffDetails);
  }
    
  // get the initial clef from the part if any
  {
    S_msrClef
      clef =
        fStaffPartUplink->
          getPartCurrentClef ();
  
    if (clef) {
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceClefs || gTraceOptions->fTraceStaves) {
        gLogIOstream <<
          "Appending part clef '" << clef->asString () <<
          "' as initial clef to staff \"" <<
          getStaffName () <<
          "\" in part " <<
          fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif

      appendClefToStaff (clef); // JMI
    }
  }
    
  // get the initial key from the part if any
  {
    //* JMI
    S_msrKey
      key =
        fStaffPartUplink->
          getPartCurrentKey ();
  
    if (key) {
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceKeys) {
        gLogIOstream <<
          "Appending part key '" << key->asString () <<
          "' as initial key to staff \"" <<
          getStaffName () <<
          "\" in part " <<
          fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif

      appendKeyToStaff (key);
    }
  }
    
  // get the initial transpose from the part if any
  {
    S_msrTranspose
      transpose =
        fStaffPartUplink->
          getPartCurrentTranspose ();
        
    if (transpose) {
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceStaves /* JMI ||       gTraceOptions->fTraceTransposes */) {
        gLogIOstream <<
          "Appending part transpose '" << transpose->asString () <<
          "' as initial transpose to staff \"" <<
          getStaffName () <<
          "\" in part " <<
          fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif
      
      fStaffCurrentTranspose = transpose;
      
      appendTransposeToAllStaffVoices (transpose);
    }
  }

  // set staff instrument names default values // JMI
  fStaffInstrumentName =
    fStaffPartUplink->
      getPartInstrumentName ();
  fStaffInstrumentAbbreviation =
    fStaffPartUplink->
      getPartInstrumentAbbreviation ();

  // rest measures
  fStaffContainsRestMeasures = false;

  gIndenter--;
}

msrStaff::~msrStaff ()
{}

S_msrStaff msrStaff::createStaffNewbornClone (
  S_msrPart containingPart)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Creating a newborn clone of staff \"" <<
      fStaffName <<
      "\"" <<
      endl;
  }
#endif
  
  // sanity check
  msrAssert(
    containingPart != nullptr,
    "containingPart is null");
    
  S_msrStaff
    newbornClone =
      msrStaff::create (
        fInputLineNumber,
        fStaffKind,
        fStaffNumber,
        containingPart);

  newbornClone->fStaffName =
    fStaffName;
    
  newbornClone->fStaffNumber =
    fStaffNumber;
    
  newbornClone->fStaffInstrumentName =
    fStaffInstrumentName;
    
  newbornClone->fStaffInstrumentAbbreviation =
    fStaffInstrumentAbbreviation;
        
  return newbornClone;
}

void msrStaff::setStaffCurrentClef (S_msrClef clef)
{
  fStaffCurrentClef = clef;
};

void msrStaff::setStaffCurrentKey (S_msrKey key)
{
  fStaffCurrentKey = key;
};

void msrStaff::setStaffCurrentTime (S_msrTime time)
{
  fStaffCurrentTime = time;
};

string msrStaff::staffNumberAsString ()
{
  string result;
  
  switch (fStaffNumber) {
    case K_PART_FIGURED_BASS_STAFF_NUMBER:
      result = "K_PART_FIGURED_BASS_STAFF_NUMBER";
      break;
    default:
      result = to_string (fStaffNumber);
  } // switch
  
  return result;
}

/* KEEP JMI
const int msrStaff::getStaffNumberOfMusicVoices () const
{
  int result = 0;

  for (
    map<int, S_msrVoice>::const_iterator i =
      fStaffRegularVoicesMap.begin ();
    i != fStaffRegularVoicesMap.end ();
    i++) {
      S_msrVoice
        voice =
          (*i).second;

      switch (voice->getVoiceKind ()) {
        case msrVoice::kRegularVoice:
          if (voice->getMusicHasBeenInsertedInVoice ()) {
            result++;
          }
          break;
          
        case msrVoice::kHarmonyVoice: // JMI
          break;
          
        case msrVoice::kFiguredBassVoice: // JMI
          break;
      } // switch
      
  } // for

  return result;
}
*/

void msrStaff::createMeasureAndAppendItToStaff (
  int    inputLineNumber,
  string measureNumber,
  msrMeasure::msrMeasureImplicitKind
         measureImplicitKind)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceMeasures || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Creating and appending measure '" <<
      measureNumber <<
      ", in staff \"" << getStaffName () << "\"" <<
      "', line " << inputLineNumber <<
      endl;
  }
#endif

  // propagate it to all staves
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++
  ) {
    S_msrVoice
      voice = (*i).second;

    // sanity check
    msrAssert (
      voice != nullptr,
      "voice is null");
    
    switch (voice->getVoiceKind ()) {
      case msrVoice::kRegularVoice:
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceMeasures || gTraceOptions->fTraceStaves) {
          gLogIOstream <<
            "Propagating the creation and appending of measure '" <<
            measureNumber <<
            "', line " << inputLineNumber <<
            ", to voice \"" << voice->getVoiceName () << "\"" <<
            endl;
        }
#endif
  
        voice->
          createMeasureAndAppendItToVoice (
            inputLineNumber,
            measureNumber,
            measureImplicitKind);
        break;
        
      case msrVoice::kHarmonyVoice:
        break;
        
      case msrVoice::kFiguredBassVoice:
        break;
    } // switch
  } // for
}

void msrStaff::setNextMeasureNumberInStaff (
  int    inputLineNumber,
  string nextMeasureNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceMeasures || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Setting next measure number to '" <<
      nextMeasureNumber <<
      ", in staff \"" << getStaffName () << "\"" <<
      "', line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  // propagate it to all staves
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    S_msrVoice voice = (*i).second;

    // sanity check
    msrAssert (
      voice != nullptr,
      "voice is null");
    
#ifdef TRACE_OPTIONS
    if (gTraceOptions->fTraceMeasures || gTraceOptions->fTraceStaves) {
      gLogIOstream <<
        "Propagating the setting of next measure number '" <<
        nextMeasureNumber <<
        "', line " << inputLineNumber <<
        ", in voice \"" << voice->getVoiceName () << "\"" <<
        endl;
    }
#endif

    voice->
      setNextMeasureNumberInVoice (
        inputLineNumber,
        nextMeasureNumber);
  } // for

  gIndenter--;
}

S_msrVoice msrStaff::createVoiceInStaffByItsNumber (
  int                    inputLineNumber,
  msrVoice::msrVoiceKind voiceKind,
  int                    voiceNumber,
  string                 currentMeasureNumber)
{
  // take this new voice into account if relevant
  switch (voiceKind) {
    case msrVoice::kRegularVoice:
      fStaffRegularVoicesCounter++;

#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
        gLogIOstream <<
          "Creating regular voice number '" <<
          voiceNumber <<
          "', voiceKind '" <<
          msrVoice::voiceKindAsString (voiceKind) <<
          "' as regular voice '" <<
          fStaffRegularVoicesCounter <<
          "' of staff \"" << getStaffName () <<
          "\", line " << inputLineNumber <<
          "\", current measure number: " <<
          currentMeasureNumber <<
     // JMI     " in part " << fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif
      break;
      
    case msrVoice::kHarmonyVoice:
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
        gLogIOstream <<
          "Creating harmony voice number '" <<
          voiceNumber <<
          "', voiceKind '" <<
          msrVoice::voiceKindAsString (voiceKind) <<
          "' in staff \"" << getStaffName () <<
          "\", line " << inputLineNumber <<
          "\", current measure number: " <<
          currentMeasureNumber <<
     // JMI     " in part " << fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif
      break;
      
    case msrVoice::kFiguredBassVoice:
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
        gLogIOstream <<
          "Creating figured bass voice number '" <<
          voiceNumber <<
          "', voiceKind '" <<
          msrVoice::voiceKindAsString (voiceKind) <<
          "' in staff \"" << getStaffName () <<
          "\", line " << inputLineNumber <<
          "\", current measure number: " <<
          currentMeasureNumber <<
     // JMI     " in part " << fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif
      break;
  } // switch


  // are there too many regular voices in this staff? 
  if (fStaffRegularVoicesCounter > msrStaff::gStaffMaxRegularVoices) {
    stringstream s;
    
    s <<
      "staff \"" << getStaffName () <<
      "\" is already filled up with " <<
      msrStaff::gStaffMaxRegularVoices << " regular voices" <<
      endl <<
      ". voice number " <<
      voiceNumber <<
      " overflows it" <<
      endl <<
      ", fStaffRegularVoicesCounter = " <<
      fStaffRegularVoicesCounter <<
      ", msrStaff::gStaffMaxRegularVoices = " <<
      msrStaff::gStaffMaxRegularVoices <<
      endl;

      /* JMI ???
    msrMusicXMLError (
// JMI    msrMusicXMLWarning ( JMI
      gGeneralOptions->fInputSourceName,
      inputLineNumber,
      __FILE__, __LINE__,
      s.str ());
      */
  }

  // create the voice
  S_msrVoice
    voice =
      msrVoice::create (
        inputLineNumber,
        voiceKind,
        voiceNumber,
        msrVoice::kCreateInitialLastSegmentYes,
        this);
          
  // take this new voice into account if relevant
  switch (voiceKind) {      
    case msrVoice::kRegularVoice:
      // register the voice by its relative number
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceVoices) {
        gLogIOstream <<
          "Voice " << voiceNumber <<
          " in staff " << getStaffName () <<
          " gets staff regular voice number " <<
          fStaffRegularVoicesCounter <<
          endl;
      }
#endif
        
      registerVoiceInRegularVoicesMap (
        voiceNumber,
        voice);
      break;

    case msrVoice::kHarmonyVoice:
      break;
            
    case msrVoice::kFiguredBassVoice:
      break;
  } // switch

  // register it in staff by its number
  registerVoiceByItsNumber (
    inputLineNumber,
    voiceNumber,
    voice);
    
  return voice;
}

void msrStaff::registerVoiceByItsNumber (
  int        inputLineNumber,
  int        voiceNumber,
  S_msrVoice voice)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceVoices || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Registering voice number '" << voiceNumber <<
      "', named \"" << voice->getVoiceName () <<
      "\" in staff " << getStaffName () <<
      endl;
  }
#endif

  // register voice in all voices map
  fStaffAllVoicesMap [voiceNumber] =
    voice;

  // register it in all voices list
  fStaffAllVoicesList.push_back (voice);

  // sort the list if necessary
  switch (voice->getVoiceKind ()) {      
    case msrVoice::kRegularVoice:
      break;

    case msrVoice::kHarmonyVoice:
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
        gLogIOstream <<
          "Sorting the voices in staff \"" <<
          getStaffName () << "\"" <<
          ", line " << inputLineNumber <<
          endl;
      }
#endif

    /* JMI
      gLogIOstream <<
        endl <<
        endl <<
        "@@@@@@@@@@@@@@@@ fStaffAllVoicesList contains initially:" <<
        endl <<
        endl;
    
      for (
        list<S_msrVoice>::const_iterator i = fStaffAllVoicesList.begin ();
        i != fStaffAllVoicesList.end ();
        i++) {
        S_msrVoice
          voice = (*i);
          
        gLogIOstream <<
          voice->getVoiceName () <<
          endl;
      } // for
      gLogIOstream <<
        endl <<
        endl;
      */
      
      // sort fStaffAllVoicesList, to have harmonies just before
      // the corresponding voice
      if (fStaffAllVoicesList.size ()) {
        fStaffAllVoicesList.sort (
          compareVoicesToHaveHarmoniesAboveCorrespondingVoice);
      }

    /* JMI
      gLogIOstream <<
        endl <<
        endl <<
        "@@@@@@@@@@@@@@@@ fStaffAllVoicesList contains after sort:" <<
        endl <<
        endl;
    
      for (
        list<S_msrVoice>::const_iterator i = fStaffAllVoicesList.begin ();
        i != fStaffAllVoicesList.end ();
        i++) {
        S_msrVoice
          voice = (*i);
          
        gLogIOstream <<
          voice->getVoiceName () <<
          endl;
      } // for
      gLogIOstream <<
        endl <<
        endl;
        */
      break;
            
    case msrVoice::kFiguredBassVoice:
      break;
  } // switch  
}

void msrStaff::registerVoiceInRegularVoicesMap (
  int        voiceNumber,
  S_msrVoice voice)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceVoices || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Registering regular voice number '" << voiceNumber <<
      "', named \"" << voice->getVoiceName () <<
      "\" in staff " << getStaffName () <<
      "'s regular voices map as regular voice with sequential number " <<
      fStaffRegularVoicesCounter <<
      endl;
  }
#endif

  fStaffRegularVoicesMap [fStaffRegularVoicesCounter] =
    voice;

  // set voice staff sequential number
  voice->
    setRegularVoiceStaffSequentialNumber (
      fStaffRegularVoicesCounter);
}

void msrStaff::registerVoiceInAllVoicesList (
  int        voiceNumber,
  S_msrVoice voice)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceVoices || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Registering voice number '" << voiceNumber <<
      "', named \"" << voice->getVoiceName () <<
      "\" in staff " << getStaffName () <<
      "'s all voices list" <<
      endl;
  }
#endif

  fStaffAllVoicesList.push_back (voice);
}

S_msrVoice msrStaff::fetchVoiceFromStaffByItsNumber (
  int inputLineNumber,
  int voiceNumber)
{
  S_msrVoice result; // JMI avoid repetitive messages!

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceVoices || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Fetching voice number " <<
      voiceNumber <<
     " in staff \"" << getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i =
      fStaffRegularVoicesMap.begin ();
    i != fStaffRegularVoicesMap.end ();
    i++) {
    if (
      (*i).second->getVoiceNumber ()
        ==
      voiceNumber) {
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceVoices || gTraceOptions->fTraceStaves) {
          gLogIOstream <<
            "Voice " << voiceNumber <<
            " in staff \"" << getStaffName () << "\"" <<
            " has staff relative number " << (*i).first <<
            endl;
        }
#endif
        
        result = (*i).second;
        break;
    }
  } // for

  return result;
}

void msrStaff::addAVoiceToStaffIfItHasNone (
  int inputLineNumber)
{
  if (fStaffAllVoicesMap.size () == 0) {
#ifdef TRACE_OPTIONS
    if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
      gLogIOstream <<
        "Staff \"" <<
        getStaffName () <<
        "\" doesn't contain any voice, adding one" <<
        ", line " << inputLineNumber <<
        endl;
    }
#endif

    this->
      createVoiceInStaffByItsNumber (
        inputLineNumber,
        msrVoice::kRegularVoice,
        1,    // voiceNumber,
        "1"); // fCurrentMeasureNumber
  }
}

void msrStaff::registerVoiceInStaff (
  int        inputLineNumber,
  S_msrVoice voice)
{
  // sanity check
  msrAssert (
    voice != nullptr,
    "voice is null");

  // get voice kind
  msrVoice::msrVoiceKind voiceKind =
    voice->getVoiceKind ();
    
  // take this new voice into account if relevant
  switch (voiceKind) {
    case msrVoice::kRegularVoice:
      // take that regular voice into account
      fStaffRegularVoicesCounter++;

      // are there too many voices in this staff? 
      if (fStaffRegularVoicesCounter > msrStaff::gStaffMaxRegularVoices) {
        stringstream s;
        
        s <<
          "staff \"" << getStaffName () <<
          "\" is already filled up with " <<
          msrStaff::gStaffMaxRegularVoices << " regular voices," <<
          endl <<
          "the voice named \"" << voice->getVoiceName () << "\" overflows it" <<
          endl <<
          ", fStaffRegularVoicesCounter = " <<
          fStaffRegularVoicesCounter <<
          ", msrStaff::gStaffMaxRegularVoices = " <<
          msrStaff::gStaffMaxRegularVoices <<
          endl;

          /* JMI ???
        msrMusicXMLError (
    // JMI    msrMusicXMLWarning ( JMI
          gGeneralOptions->fInputSourceName,
          inputLineNumber,
          __FILE__, __LINE__,
          s.str ());
          */
      }
      break;
            
    case msrVoice::kHarmonyVoice:
      break;
    case msrVoice::kFiguredBassVoice:
      break;
  } // switch

  // register voice in this staff
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
    gLogIOstream <<
      "Registering voice \"" << voice->getVoiceName () <<
      "\" as relative voice " <<
      fStaffRegularVoicesCounter <<
      " of staff \"" << getStaffName () <<
      "\", line " << inputLineNumber <<
// JMI       " in part " << fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  // register it in staff by its number
  registerVoiceByItsNumber (
    inputLineNumber,
    voice->getVoiceNumber (),
    voice);

  // is voice a regular voice???
  switch (voiceKind) {
    case msrVoice::kRegularVoice:
      {
        int voiceNumber = voice->getVoiceNumber ();
        
        // register the voice by its number
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceVoices) {
          gLogIOstream <<
            "Registering regular voice '" << voiceNumber <<
            "' " << voice->getVoiceName () <<
            " with staff regular voice number " <<
            fStaffRegularVoicesCounter <<
            " in staff " << getStaffName () <<
            endl;
        }
#endif

        registerVoiceInRegularVoicesMap (
          voiceNumber,
          voice);
      }
      break;
      
    case msrVoice::kHarmonyVoice:
      break;
      
    case msrVoice::kFiguredBassVoice:
      break;
  } // switch
}

void msrStaff::padUpToActualMeasureWholeNotesInStaff (
  int      inputLineNumber,
  rational wholeNotes)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceMeasures) {
    gLogIOstream <<
      "Padding up to actual measure whole notes '" << wholeNotes <<
      "' in staff \"" <<
      getStaffName () <<
      "\", line " << inputLineNumber <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
  i++) {
    S_msrVoice voice = (*i).second;
    // JMI msrAssert???
    
    voice-> 
      padUpToActualMeasureWholeNotesInVoice (
        inputLineNumber,
        wholeNotes);
  } // for
}

void msrStaff::appendClefToStaff (S_msrClef clef)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceClefs || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Appending clef '" << clef->asString () <<
      "' to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  gIndenter++;
  
  // append clef to the staff,
  // unless we should ignore redundant clefs
  // and a clef equal to the current clef is met
  bool doAppendClefToStaff = true;
    
  if (fStaffCurrentClef) {
    if (
      gMusicXMLOptions->fIgnoreRedundantClefs
        &&
      clef->isEqualTo (fStaffCurrentClef)
    ) {
      doAppendClefToStaff = false;
    }
  }
  
  if (doAppendClefToStaff) {
    // register clef as current staff clef
    fStaffCurrentClef = clef;

// JMI ??? should there be a staff lines number change for 00f-Basics-Clefs.xml???

    // set staff kind accordingly if relevant
    switch (clef->getClefKind ()) {
      case msrClef::kPercussionClef:
        fStaffKind = msrStaff::kStaffDrum; // JMI ???
        break;
      case msrClef::kTablature4Clef:
      case msrClef::kTablature5Clef:
      case msrClef::kTablature6Clef:
      case msrClef::kTablature7Clef:
        fStaffKind = msrStaff::kStaffTablature;
        break;
      default:
        ;
    } // switch

    // propagate clef to all voices
    for (
      map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
      i != fStaffAllVoicesMap.end ();
      i++
    ) {
      (*i).second-> // JMI msrAssert???
        appendClefToVoice (clef);
    } // for
  }

  else {
#ifdef TRACE_OPTIONS
    if (gTraceOptions->fTraceClefs || gTraceOptions->fTraceStaves) {
      gLogIOstream <<
        "Clef '" <<
        clef->asString () <<
        "' ignored because it is already present in staff " <<
        getStaffName () <<
        "\" in part " <<
        fStaffPartUplink->getPartCombinedName () <<
        endl;
    }
#endif
  }

  gIndenter--;
}

void msrStaff::appendKeyToStaff (S_msrKey  key)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceKeys || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Appending key '" << key->asString () <<
      "' to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  gIndenter++;
  
  // append key to staff?
  bool doAppendKeyToStaff = true;
    
  if (fStaffCurrentKey) {
    if (
      gMusicXMLOptions->fIgnoreRedundantKeys
        &&
      fStaffCurrentKey->isEqualTo (key)
    ) {
      doAppendKeyToStaff = false;
    }
    
    else {
      if (key->isEqualTo (fStaffCurrentKey)) {
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceKeys || gTraceOptions->fTraceStaves) {
          gLogIOstream <<
            "Key '" <<
            key->asString () <<
            "' ignored because it is already present in staff " <<
            getStaffName () <<
            "\" in part " <<
            fStaffPartUplink->getPartCombinedName () <<
            endl;
        }
#endif
  
        doAppendKeyToStaff = false;
      }
    }
  }
  
  if (doAppendKeyToStaff) {
    // register key as current staff key
    fStaffCurrentKey = key;
  
    // propagate it to all voices
    for (
      map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
      i != fStaffAllVoicesMap.end ();
      i++
    ) {
      (*i).second-> // JMI msrAssert???
        appendKeyToVoice (key);
    } // for
  }

  gIndenter--;
}

void msrStaff::appendTimeToStaff (S_msrTime time)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTimes || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Appending time '" << time->asString () <<
      "' to staff \"" <<
      getStaffName () <<
      "\"" <<
      endl;
  }
#endif

  gIndenter++;
  
  // append time to staff?
  bool doAppendTimeToStaff = true;

  if (fStaffCurrentTime) {
    if (
      gMusicXMLOptions->fIgnoreRedundantTimes
        &&
      fStaffCurrentTime->isEqualTo (time)
    ) {
      doAppendTimeToStaff = false;
    }
    
    else {
      if (time->isEqualTo (fStaffCurrentTime)) {
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceTimes || gTraceOptions->fTraceStaves) {
          gLogIOstream <<
            "Time '" <<
            time->asString () <<
            "' ignored because it is already present in staff " <<
            getStaffName () <<
            "\" in part " <<
            fStaffPartUplink->getPartCombinedName () <<
            endl;
        }
#endif
  
        doAppendTimeToStaff = false;
      }
    }
  }
  
  if (doAppendTimeToStaff) {
    // register time as current staff time
    fStaffCurrentTime = time;

    // propagate it to all voices
    for (
      map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
      i != fStaffAllVoicesMap.end ();
      i++
    ) {
      (*i).second-> // JMI msrAssert???
        appendTimeToVoice (time);
    } // for
  }

  gIndenter--;
}    

void msrStaff::appendTimeToStaffClone (S_msrTime time)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTimes || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Appending time '" << time->asString () <<
      "' to staff clone \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  gIndenter++;
  
  // set staff time
  fStaffCurrentTime = time;

  // propagate it to all voices
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second-> // JMI msrAssert???
      appendTimeToVoiceClone (time);
  } // for

  gIndenter--;
}    

/* JMI
void msrStaff::nestContentsIntoNewRepeatInStaff (
  int inputLineNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Nesting contents into new repeat in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      nestContentsIntoNewRepeatInVoice (
        inputLineNumber);
  } // for
}
*/

void msrStaff::handleRepeatStartInStaff (
  int inputLineNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Handling repeat start in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      handleRepeatStartInVoice (
        inputLineNumber);
  } // for

  gIndenter--;
}

void msrStaff::handleRepeatEndInStaff (
  int    inputLineNumber,
  string measureNumber,
  int    repeatTimes)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Handling a repeat end in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      handleRepeatEndInVoice (
        inputLineNumber,
        measureNumber,
        repeatTimes);
  } // for

  gIndenter--;
}

void msrStaff::handleRepeatEndingStartInStaff (
  int inputLineNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Handling a repeat ending start in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      handleRepeatEndingStartInVoice (
        inputLineNumber);
  } // for

  gIndenter--;
}

void msrStaff::handleRepeatEndingEndInStaff (
  int       inputLineNumber,
  string    repeatEndingNumber, // may be "1, 2"
  msrRepeatEnding::msrRepeatEndingKind
            repeatEndingKind)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Handling a " <<
      msrRepeatEnding::repeatEndingKindAsString (
        repeatEndingKind) <<
      " repeat ending end in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      handleRepeatEndingEndInVoice (
        inputLineNumber,
        repeatEndingNumber,
        repeatEndingKind);
  } // for

  gIndenter--;
}

void msrStaff::finalizeRepeatEndInStaff (
  int    inputLineNumber,
  string measureNumber,
  int    repeatTimes)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Finalizing a repeat upon its end in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      finalizeRepeatEndInVoice (
        inputLineNumber,
        measureNumber,
        repeatTimes);
  } // for

  gIndenter--;
}

void msrStaff::createMeasuresRepeatFromItsFirstMeasuresInStaff (
  int inputLineNumber,
  int measuresRepeatMeasuresNumber,
  int measuresRepeatSlashes)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Creating a measures repeat from it's first measure in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      createMeasuresRepeatFromItsFirstMeasuresInVoice (
        inputLineNumber,
        measuresRepeatMeasuresNumber,
        measuresRepeatSlashes);
  } // for
}

void msrStaff::appendPendingMeasuresRepeatToStaff (
  int inputLineNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Appending the pending measures repeat to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendPendingMeasuresRepeatToVoice (
        inputLineNumber);
  } // for
}

void msrStaff::createRestMeasuresInStaff (
  int inputLineNumber,
  int restMeasuresMeasuresNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Creating a multiple rest in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", " <<
      singularOrPlural (
        restMeasuresMeasuresNumber, "measure", "measures") <<
      endl;
  }
#endif

  fStaffContainsRestMeasures = true;

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      createRestMeasuresInVoice (
        inputLineNumber,
        restMeasuresMeasuresNumber);
  } // for
}

void msrStaff::appendPendingRestMeasuresToStaff (
  int inputLineNumber)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Appending the pending multiple rest to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendPendingRestMeasuresToVoice (
        inputLineNumber);
  } // for
}

void msrStaff::appendRestMeasuresCloneToStaff (
  int               inputLineNumber,
  S_msrRestMeasures restMeasures)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Appending multiple rest '" <<
      restMeasures->asString () <<
      "' to staff clone \"" <<
      getStaffName () <<
      "\"" <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second-> // JMI msrAssert???
      appendRestMeasuresCloneToVoiceClone (
        inputLineNumber,
        restMeasures);
  } // for
}

void msrStaff::appendRepeatCloneToStaff (
  int         inputLineNumber,
  S_msrRepeat repeatCLone)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Appending repeat clone to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second-> // JMI msrAssert???
      appendRepeatCloneToVoiceClone (
        inputLineNumber, repeatCLone);
  } // for
}

void msrStaff::appendRepeatEndingCloneToStaff (
  S_msrRepeatEnding repeatEndingClone)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    gLogIOstream <<
      "Appending a repeat ending clone to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      ", line " << repeatEndingClone->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second-> // JMI msrAssert???
      appendRepeatEndingCloneToVoice (
        repeatEndingClone);
  } // for

  gIndenter--;
}

void msrStaff::appendBarlineToStaff (S_msrBarline barline)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceMeasures) {
    gLogIOstream <<
      "Appending barline '" << barline->asString () <<
      "' to staff " <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  gIndenter++;
  
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendBarlineToVoice (barline);
  } // for

  gIndenter--;
}

void msrStaff::appendTransposeToStaff (
  S_msrTranspose transpose)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTranspositions || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Setting transpose '" <<
      transpose->asString () <<
      "' in staff " <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  // set staff transpose
  bool doAppendTransposeToStaff = true;
  
  if (! fStaffCurrentTranspose) {
    doAppendTransposeToStaff = true;
  }
  
  else {
    if (transpose->isEqualTo (fStaffCurrentTranspose)) {
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceTranspositions || gTraceOptions->fTraceStaves) {
        gLogIOstream <<
          "Transpose '" <<
          transpose->asString () <<
          "' ignored because it is already present in staff " <<
          getStaffName () <<
          "\" in part " <<
          fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif

      doAppendTransposeToStaff = false;
    }
  }
  
  if (doAppendTransposeToStaff) {
    // register transpose as current staff transpose
    fStaffCurrentTranspose = transpose;
  
    // propagate it to all voices
    appendTransposeToAllStaffVoices (transpose);
  }
}

void msrStaff::appendPartNameDisplayToStaff (
  S_msrPartNameDisplay partNameDisplay)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTranspositions || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Setting part name display '" <<
      partNameDisplay->asString () <<
      "' in staff " <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  // set staff transpose
  bool doAppendPartNameDisplayToStaff = true;

  /* JMI ???
  if (! fStaffCurrentTranspose) {
    doAppendPartNameDisplayToStaff = true;
  }
  
  else {
    if (partNameDisplay->isEqualTo (fStaffCurrentTranspose)) {
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceTranspositions || gTraceOptions->fTraceStaves) {
        gLogIOstream <<
          "Transpose '" <<
          partNameDisplay->asString () <<
          "' ignored because it is already present in staff " <<
          getStaffName () <<
          "\" in part " <<
          fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif

      doAppendPartNameDisplayToStaff = false;
    }
  }
  */
  
  if (doAppendPartNameDisplayToStaff) {
    // register transpose as current staff transpose
 // JMI   fStaffCurrentTranspose = partNameDisplay;
  
    // propagate it to all voices
    appendPartNameDisplayToAllStaffVoices (partNameDisplay);
  }
}

void msrStaff::appendPartAbbreviationDisplayToStaff (
  S_msrPartAbbreviationDisplay partAbbreviationDisplay)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTranspositions || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Setting part abbreviation display '" <<
      partAbbreviationDisplay->asString () <<
      "' in staff " <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  // set staff transpose
  bool doAppendPartAbbreviationDisplayToStaff = true;
  
/* JMI ???
  if (! fStaffCurrentTranspose) {
    doAppendPartAbbreviationDisplayToStaff = true;
  }
  
  else {
    if (partAbbreviationDisplay->isEqualTo (fStaffCurrentTranspose)) {
#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTraceTranspositions || gTraceOptions->fTraceStaves) {
        gLogIOstream <<
          "Transpose '" <<
          transpose->asString () <<
          "' ignored because it is already present in staff " <<
          getStaffName () <<
          "\" in part " <<
          fStaffPartUplink->getPartCombinedName () <<
          endl;
      }
#endif

      doAppendPartAbbreviationDisplayToStaff = false;
    }
  }
  */
  
  if (doAppendPartAbbreviationDisplayToStaff) {
    // register transpose as current staff transpose
 // JMI   fStaffCurrentTranspose = partAbbreviationDisplay;
  
    // propagate it to all voices
    appendPartAbbreviationDisplayToAllStaffVoices (partAbbreviationDisplay);
  }
}

void msrStaff::appendStaffDetailsToStaff (
  S_msrStaffDetails staffDetails)
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Appending staff details '" <<
      staffDetails->asShortString () <<
      "' to staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  // sanity check
  msrAssert (
    staffDetails != nullptr,
    "staffDetails is null");
    
  // register staff details in staff
  fStaffStaffDetails = staffDetails;

  // set staff kind accordingly if relevant
  switch (staffDetails->getStaffLinesNumber ()) {
    case 1:
      if (gMsrOptions->fCreateSingleLineStavesAsRythmic) {
        fStaffKind = msrStaff::kStaffRythmic;
      }
      else {
        fStaffKind = msrStaff::kStaffDrum;
      }
      break;
    default:
      ;
  } // switch
  
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Setting staff kind to '" <<
      staffKindAsString (fStaffKind) <<
      "' in staff \"" <<
      getStaffName () <<
      "\" in part " <<
      fStaffPartUplink->getPartCombinedName () <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendStaffDetailsToVoice (staffDetails);
  } // for
}

void msrStaff::appendTransposeToAllStaffVoices (
  S_msrTranspose transpose)
{
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendTransposeToVoice (transpose);
  } // for
}

void msrStaff::appendPartNameDisplayToAllStaffVoices (
  S_msrPartNameDisplay partNameDisplay)
{
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendPartNameDisplayToVoice (partNameDisplay);
  } // for
}

void msrStaff::appendPartAbbreviationDisplayToAllStaffVoices (
  S_msrPartAbbreviationDisplay partAbbreviationDisplay)
{
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendPartAbbreviationDisplayToVoice (partAbbreviationDisplay);
  } // for
}

void msrStaff::appendScordaturaToStaff (
  S_msrScordatura scordatura)
{
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendScordaturaToVoice (
        scordatura);
  } // for
}

void msrStaff::appendAccordionRegistrationToStaff (
  S_msrAccordionRegistration
    accordionRegistration)
{
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendAccordionRegistrationToVoice (
        accordionRegistration);
  } // for
}

void msrStaff::appendHarpPedalsTuningToStaff (
  S_msrHarpPedalsTuning
    harpPedalsTuning)
{
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    (*i).second->
      appendHarpPedalsTuningToVoice (
        harpPedalsTuning);
  } // for
}

void msrStaff::finalizeCurrentMeasureInStaff (
  int inputLineNumber)
{
#ifdef TRACE_OPTIONS
  rational
    partActualMeasureWholeNotesHighTide =
      fStaffPartUplink->
        getPartActualMeasureWholeNotesHighTide ();
#endif
      
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceMeasures || gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Finalizing current measure in staff \"" <<
      getStaffName () <<
      "\", line " << inputLineNumber <<
      ", partActualMeasureWholeNotesHighTide = " <<
      partActualMeasureWholeNotesHighTide <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  // finalize all the registered voices
  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    S_msrVoice
      voice =
        (*i).second;

    switch (voice->getVoiceKind ()) {
      case msrVoice::kRegularVoice:
      case msrVoice::kHarmonyVoice:
      case msrVoice::kFiguredBassVoice:
        voice->
          finalizeCurrentMeasureInVoice (
            inputLineNumber);
        break;
    } // switch
  } // for

  gIndenter--;
}

bool msrStaff::compareVoicesToHaveHarmoniesAboveCorrespondingVoice (
  const S_msrVoice& first,
  const S_msrVoice& second)
{
  int
    firstVoiceNumber =
      first->getVoiceNumber (),
    secondVoiceNumber =
      second->getVoiceNumber ();

  if (firstVoiceNumber > K_VOICE_HARMONY_VOICE_BASE_NUMBER) {
    firstVoiceNumber -= K_VOICE_HARMONY_VOICE_BASE_NUMBER + 1;
  }
  if (secondVoiceNumber > K_VOICE_HARMONY_VOICE_BASE_NUMBER) {
    secondVoiceNumber -= K_VOICE_HARMONY_VOICE_BASE_NUMBER + 1;
  }

  bool result =
    firstVoiceNumber < secondVoiceNumber;

  return result;

  /* JMI
  switch (firstVoiceNumber) {
    case msrVoice::kRegularVoice:
      switch (secondVoiceNumber) {
        case msrVoice::kRegularVoice:
          break;
  
        case msrVoice::kHarmonyVoice:
          result =
            secondVoiceNumber - K_VOICE_HARMONY_VOICE_BASE_NUMBER
              >
            firstVoiceNumber;
          break;
  
        case msrVoice::kFiguredBassVoice:
          break;
      } // switch
      break;

    case msrVoice::kHarmonyVoice:
      switch (secondVoiceNumber) {
        case msrVoice::kRegularVoice:
          result =
            firstVoiceNumber - K_VOICE_HARMONY_VOICE_BASE_NUMBER
              >
            secondVoiceNumber;
          break;
  
        case msrVoice::kHarmonyVoice:
          break;
  
        case msrVoice::kFiguredBassVoice:
          break;
      } // switch
      break;

    case msrVoice::kFiguredBassVoice:
      switch (secondVoiceNumber) {
        case msrVoice::kRegularVoice:
          break;
  
        case msrVoice::kHarmonyVoice:
          break;
  
        case msrVoice::kFiguredBassVoice:
          break;
      } // switch
      break;
  } // switch

  return result;
  */
}

void msrStaff::finalizeStaff (int inputLineNumber)
{  
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves) {
    gLogIOstream <<
      "Finalizing staff \"" <<
      getStaffName () << "\"" <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  gIndenter++;
  
  // finalize the voices
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaves || gTraceOptions->fTraceVoices) {
    gLogIOstream <<
      "Finalizing the voices in staff \"" <<
      getStaffName () << "\"" <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  for (
    map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
    i != fStaffAllVoicesMap.end ();
    i++) {
    S_msrVoice
      voice = (*i).second;

    voice->
      finalizeVoice (
        inputLineNumber);
  } // for

  gIndenter--;
}

void msrStaff::acceptIn (basevisitor* v)
{
  if (gMsrOptions->fTraceMsrVisitors) {
    gLogIOstream <<
      "% ==> msrStaff::acceptIn ()" <<
      endl;
  }
      
  if (visitor<S_msrStaff>*
    p =
      dynamic_cast<visitor<S_msrStaff>*> (v)) {
        S_msrStaff elem = this;
        
        if (gMsrOptions->fTraceMsrVisitors) {
          gLogIOstream <<
            "% ==> Launching msrStaff::visitStart ()" <<
            endl;
        }
        p->visitStart (elem);
  }
}

void msrStaff::acceptOut (basevisitor* v)
{
  if (gMsrOptions->fTraceMsrVisitors) {
    gLogIOstream <<
      "% ==> msrStaff::acceptOut ()" <<
      endl;
  }

  if (visitor<S_msrStaff>*
    p =
      dynamic_cast<visitor<S_msrStaff>*> (v)) {
        S_msrStaff elem = this;
      
        if (gMsrOptions->fTraceMsrVisitors) {
          gLogIOstream <<
            "% ==> Launching msrStaff::visitEnd ()" <<
            endl;
        }
        p->visitEnd (elem);
  }
}

void msrStaff::browseData (basevisitor* v)
{
  if (gMsrOptions->fTraceMsrVisitors) {
    gLogIOstream <<
      "% ==> msrStaff::browseData ()" <<
      endl;
  }

  /*
    fPartCurrentClef, fPartCurrentKey and fPartCurrentTime are used
    to populate newly created voices, not to create music proper:
    they're thus not browsed
  */

  /*
    fCurrentPartStaffDetails is used
    to populate newly created voices, not to create music proper:
    it is thus not browsed
  */

/*
  if (fStaffTuningsList.size ()) {
    for (
      list<S_msrStaffTuning>::const_iterator i = fStaffTuningsList.begin ();
      i != fStaffTuningsList.end ();
      i++) {
      // browse the voice
      msrBrowser<msrStaffTuning> browser (v);
      browser.browse (*(*i));
    } // for
 //   gInfgdenter--;
  }
*/

/* JMI may be useful???
  if (fStaffAllVoicesMap.size ()) {
    for (
      map<int, S_msrVoice>::const_iterator i = fStaffAllVoicesMap.begin ();
      i != fStaffAllVoicesMap.end ();
      i++) {
        msrBrowser<msrVoice> browser (v);
        browser.browse (*((*i).second));
    } // for
  }
  */

  if (fStaffAllVoicesList.size ()) {
    for (
      list<S_msrVoice>::const_iterator i = fStaffAllVoicesList.begin ();
      i != fStaffAllVoicesList.end ();
      i++) {
        msrBrowser<msrVoice> browser (v);
        browser.browse (*(*i));
    } // for
  }

  if (gMsrOptions->fTraceMsrVisitors) {
    gLogIOstream <<
      "% <== msrStaff::browseData ()" <<
      endl;
  }
}

string msrStaff::staffKindAsString (
  msrStaffKind staffKind)
{
  string result;
  
  switch (staffKind) {
    case msrStaff::kStaffRegular:
      result = "staffRegular";
      break;
    case msrStaff::kStaffTablature:
      result = "staffTablature";
      break;
    case msrStaff::kStaffHarmony:
      result = "staffHarmony";
      break;
    case msrStaff::kStaffFiguredBass:
      result = "staffFiguredBass bass";
      break;
    case msrStaff::kStaffDrum:
      result = "staffDrum";
      break;
    case msrStaff::kStaffRythmic:
      result = "staffRythmic";
      break;
  } // switch

  return result;
}

string msrStaff::staffKindAsString () const
{
  return staffKindAsString (fStaffKind);
}

void msrStaff::print (ostream& os)
{
  os <<
    "Staff " << getStaffName () <<
    ", " << staffKindAsString () <<
    ", " <<
    singularOrPlural (
      fStaffAllVoicesMap.size (), "voice", "voices") <<
    ", " <<
    singularOrPlural (
      fStaffRegularVoicesCounter,
      "regular voice",
      "regular voices") << // JMI
    ")" <<
    endl;

  gIndenter++;

  const int fieldWidth = 28;
  
  os <<
    setw (fieldWidth) <<
    "staffNumber" << " : " <<
    fStaffNumber <<
    endl <<
    setw (fieldWidth) <<
    "staffPartUplink" << " : " <<
    fStaffPartUplink->getPartCombinedName () <<
    endl <<
    setw (fieldWidth) <<
    "staffInstrumentName" << " : \"" <<
    fStaffInstrumentName <<
    "\"" <<
    endl <<
    setw (fieldWidth) <<
    "staffInstrumentAbbreviation" << " : \"" <<
    fStaffInstrumentAbbreviation <<
    "\"" <<
    endl;

  // print current the staff clef if any
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceClefs) {
    os << left <<
      setw (fieldWidth) <<
      "staffCurrentClef" << " : ";

    if (fStaffCurrentClef) {
      os <<
        "'" <<
        fStaffCurrentClef->asShortString () <<
        "'";
    }
    else {
      os <<
        "none";
    }

    os <<
      endl;
  }
#endif
  
  // print the current staff key if any
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceKeys) {
    os << left <<
      setw (fieldWidth) <<
      "staffCurrentKey" << " : ";

    if (fStaffCurrentKey) {
      os <<
        "'" <<
        fStaffCurrentKey->asShortString () <<
        "'";
    }
    else {
      os <<
        "none";
    }

    os <<
      endl;
  }
#endif
  
  // print the current staff time if any
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTimes) {
    os << left <<
      setw (fieldWidth) <<
      "staffCurrentTime" << " : ";

    if (fStaffCurrentTime) {
      os <<
        "'" <<
        fStaffCurrentTime->asShortString () <<
        "'";
    }
    else {
      os <<
        "none";
    }

    os <<
      endl;
  }
#endif
  
  // print the staff details if any
  if (fStaffStaffDetails) {
    os <<
      fStaffStaffDetails;
  }
  else {
    os << left <<
      setw (fieldWidth) <<
      "staffStaffDetails" << " : " << "none";
  }
  os <<
    endl;

/* JMI
  // print the staff tunings if any
  if (fStaffTuningsList.size ()) {
    os <<
      "staffTuningsList:" <<
      endl;
      
    list<S_msrStaffTuning>::const_iterator
      iBegin = fStaffTuningsList.begin (),
      iEnd   = fStaffTuningsList.end (),
      i      = iBegin;
      
    gIndenter++;
    for ( ; ; ) {
      os <<
        (*i)->asString ();
      if (++i == iEnd) break;
      os << endl;
    } // for
    os << endl;
    gIndenter--;
  }
  os <<
    endl;
*/

  // print the all voices map
  if (fStaffAllVoicesMap.size ()) {
    os <<
      "staffAllVoicesMap" <<
      endl;
      
    gIndenter++;

    map<int, S_msrVoice>::const_iterator
      iBegin = fStaffAllVoicesMap.begin (),
      iEnd   = fStaffAllVoicesMap.end (),
      i      = iBegin;
      
    for ( ; ; ) {
      int        voiceNumber = (*i).first;
      S_msrVoice voice       = (*i).second;
      
        os <<
          voiceNumber << " : " <<
          "regularVoiceStaffSequentialNumber = " <<
          voice->getRegularVoiceStaffSequentialNumber () <<
          ", " <<
          voice->asShortString ();
        if (++i == iEnd) break;
        os << endl;
    } // for
    os <<
      endl;
    
    gIndenter--;
  }

  // print the regular voices map
  if (fStaffAllVoicesMap.size ()) {
    os <<
      "staffRegularVoicesMap" <<
      endl;
      
    gIndenter++;

    map<int, S_msrVoice>::const_iterator
      iBegin = fStaffRegularVoicesMap.begin (),
      iEnd   = fStaffRegularVoicesMap.end (),
      i      = iBegin;
      
    for ( ; ; ) {
      int        voiceNumber = (*i).first;
      S_msrVoice voice       = (*i).second;

      // sanity check
      msrAssert (
        voice != nullptr,
        "voice is null");
      os <<
        voiceNumber << " : " <<
        "regularVoiceStaffSequentialNumber = " <<
        voice->getRegularVoiceStaffSequentialNumber () <<
        ", " <<
        voice->asShortString ();
      if (++i == iEnd) break;
      os << endl;
    } // for
    os <<
      endl;
    
    gIndenter--;
  }

  // print the  voices
  if (fStaffAllVoicesMap.size ()) {
    os <<
      endl;
    
    map<int, S_msrVoice>::const_iterator
      iBegin = fStaffAllVoicesMap.begin (),
      iEnd   = fStaffAllVoicesMap.end (),
      i      = iBegin;
      
    for ( ; ; ) {
      S_msrVoice voice = (*i).second;

/* JMI
os <<
  endl <<
  "================= voice :" <<
  endl <<
  voice <<
  endl <<
  endl;
*/

      os <<
        voice;

        /* JMI
      switch (voice->getVoiceKind ()) {
        case msrVoice::kRegularVoice:
          os <<
            voice;
          break;

        case msrVoice::kHarmonyVoice:
          if (
            gMsrOptions->fShowHarmonyVoices
              ||
            voice->getMusicHasBeenInsertedInVoice ())
            os <<
              voice;
          break;
          
        case msrVoice::kFiguredBassVoice:
          if (
            gMsrOptions->fShowFiguredBassVoices
              ||
            voice->getMusicHasBeenInsertedInVoice ())
            os <<
              voice;
          break;
      } // switch
        */

      if (++i == iEnd) break;

      os <<
        endl;
    } // for
  }

  gIndenter--;
}

void msrStaff::printSummary (ostream& os)
{
  os <<
    "Staff" " " << getStaffName () <<
    ", " << staffKindAsString () <<
    " (" <<
    singularOrPlural (
      fStaffAllVoicesMap.size (), "voice", "voices") <<
    ")" <<
    endl;

  gIndenter++;

  os <<
    "StaffInstrumentName: \"" <<
    fStaffInstrumentName << "\"" <<
    endl;

/* JMI
  if (fStaffTuningsList.size ()) {
    os <<
      "Staff tunings:" <<
      endl;
      
    list<S_msrStaffTuning>::const_iterator
      iBegin = fStaffTuningsList.begin (),
      iEnd   = fStaffTuningsList.end (),
      i      = iBegin;
      
    gIndenter++;
    for ( ; ; ) {
      os << (*i)->asString ();
      if (++i == iEnd) break;
      os << endl;
    } // for
    os << endl;
    gIndenter--;
  }
*/

  // print the voices names
  if (fStaffAllVoicesMap.size ()) {
    os <<
      "Voices:" <<
      endl;
  
    gIndenter++;
    
    map<int, S_msrVoice>::const_iterator
      iBegin = fStaffAllVoicesMap.begin (),
      iEnd   = fStaffAllVoicesMap.end (),
      i      = iBegin;
      
    for ( ; ; ) {
      S_msrVoice
        voice =
          (*i).second;
          
      os <<
        left <<
          voice->getVoiceName () <<
          " (" <<
          singularOrPlural (
            voice->getVoiceActualNotesCounter (),
            "actual note",
            "actual notes") <<
          ", " <<
          singularOrPlural (
            voice->getVoiceStanzasMap ().size (),
            "stanza",
            "stanzas") <<
          ")";
      if (++i == iEnd) break;
      os << endl;
    } // for

    gIndenter --;
  }

  gIndenter--;

  os <<
    endl;
}

ostream& operator<< (ostream& os, const S_msrStaff& elt)
{
  elt->print (os);
  return os;
}

//______________________________________________________________________________
S_msrVoiceStaffChange msrVoiceStaffChange::create (
  int        inputLineNumber,
  S_msrStaff staffToChangeTo)
{
  msrVoiceStaffChange* o =
    new msrVoiceStaffChange (
      inputLineNumber, staffToChangeTo);
  assert(o!=0);
  return o;
}

msrVoiceStaffChange::msrVoiceStaffChange (
  int        inputLineNumber,
  S_msrStaff staffToChangeTo)
    : msrMeasureElement (inputLineNumber)
{
  fStaffToChangeTo = staffToChangeTo;
}

msrVoiceStaffChange::~msrVoiceStaffChange ()
{}

S_msrVoiceStaffChange msrVoiceStaffChange::createStaffChangeNewbornClone ()
{
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceStaffTuning) {
    gLogIOstream <<
      "Creating a newborn clone of staff change '" <<
      asString () <<
      "'" <<
      endl;
  }
#endif

 S_msrVoiceStaffChange
    newbornClone =
      msrVoiceStaffChange::create (
        fInputLineNumber,
        fStaffToChangeTo);
  
  return newbornClone;
}

void msrVoiceStaffChange::acceptIn (basevisitor* v)
{
  if (gMsrOptions->fTraceMsrVisitors) {
    gLogIOstream <<
      "% ==> msrVoiceStaffChange::acceptIn ()" <<
      endl;
  }
      
  if (visitor<S_msrVoiceStaffChange>*
    p =
      dynamic_cast<visitor<S_msrVoiceStaffChange>*> (v)) {
        S_msrVoiceStaffChange elem = this;
        
        if (gMsrOptions->fTraceMsrVisitors) {
          gLogIOstream <<
            "% ==> Launching msrVoiceStaffChange::visitStart ()" <<
            endl;
        }
        p->visitStart (elem);
  }
}

void msrVoiceStaffChange::acceptOut (basevisitor* v)
{
  if (gMsrOptions->fTraceMsrVisitors) {
    gLogIOstream <<
      "% ==> msrVoiceStaffChange::acceptOut ()" <<
      endl;
  }

  if (visitor<S_msrVoiceStaffChange>*
    p =
      dynamic_cast<visitor<S_msrVoiceStaffChange>*> (v)) {
        S_msrVoiceStaffChange elem = this;
      
        if (gMsrOptions->fTraceMsrVisitors) {
          gLogIOstream <<
            "% ==> Launching msrVoiceStaffChange::visitEnd ()" <<
            endl;
        }
        p->visitEnd (elem);
  }
}

void msrVoiceStaffChange::browseData (basevisitor* v)
{}

string msrVoiceStaffChange::asString () const
{
  stringstream s;

  s <<
    "VoiceStaffChange" <<
    ", line " << fInputLineNumber <<
    ", " <<
    "staffToChangeTo: \"" << fStaffToChangeTo->getStaffName () << "\"";
    
  return s.str ();
}

void msrVoiceStaffChange::print (ostream& os)
{
  os <<
    asString () <<
    endl;
}

ostream& operator<< (ostream& os, const S_msrVoiceStaffChange& elt)
{
  elt->print (os);
  return os;
}


}
