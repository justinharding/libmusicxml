/*
  MusicXML Library
  Copyright (C) Grame 2006-2013

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr
*/

#include <iomanip>      // setw, setprecision, ...
#include <cmath>
#include <string>

#include "generalOptions.h"

#include "setTraceOptionsIfDesired.h"
#ifdef TRACE_OPTIONS
  #include "traceOptions.h"
#endif

#include "musicXMLOptions.h"
#include "lilypondOptions.h"

#include "lpsr2LilypondTranslator.h"


using namespace std;

namespace MusicXML2
{

// for comments highlighting in the generated Lilypond code
const int commentFieldWidth = 30;

//______________________________________________________________________________
S_lpsrRepeatDescr lpsrRepeatDescr::create (
  int repeatEndingsNumber)
{
  lpsrRepeatDescr* o = new
    lpsrRepeatDescr (
      repeatEndingsNumber);
  assert(o!=0);
  return o;
}

lpsrRepeatDescr::lpsrRepeatDescr (
  int repeatEndingsNumber)
{
  fRepeatEndingsNumber  = repeatEndingsNumber;
  fRepeatEndingsCounter = 0;

  fEndOfRepeatHasBeenGenerated = false;
}

lpsrRepeatDescr::~lpsrRepeatDescr ()
{}

string lpsrRepeatDescr::repeatDescrAsString () const
{
  stringstream s;

  s <<
    "fRepeatEndingsNumber = " <<
    fRepeatEndingsNumber <<
    ", fRepeatEndingsCounter = " <<
    fRepeatEndingsCounter <<
    ", fEndOfRepeatHasBeenGenerated = " <<
    booleanAsString (
      fEndOfRepeatHasBeenGenerated);

  return s.str ();
}

void lpsrRepeatDescr::print (ostream& os) const
{
  const int fieldWidth = 29;

  os << left <<
    setw (fieldWidth) <<
    "fRepeatEndingsNumber" << " : " <<
    fRepeatEndingsNumber <<
    endl <<
    setw (fieldWidth) <<
    "fRepeatEndingsCounter" << " : " <<
    fRepeatEndingsCounter <<
    endl <<
    setw (fieldWidth) <<
    "fEndOfRepeatHasBeenGenerated" << " : " <<
    fEndOfRepeatHasBeenGenerated <<
    endl;
}

ostream& operator<< (ostream& os, const S_lpsrRepeatDescr& elt)
{
  elt->print (os);
  return os;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::setCurrentOctaveEntryReferenceFromTheLilypondOptions ()
{
  if (gLilypondOptions->fRelativeOctaveEntrySemiTonesPitchAndOctave) {
    // option '-rel, -relative' has been used:
    fCurrentOctaveEntryReference =
      msrNote::createNoteFromSemiTonesPitchAndOctave (
        K_NO_INPUT_LINE_NUMBER,
        gLilypondOptions->fRelativeOctaveEntrySemiTonesPitchAndOctave);
  }
  else {
    fCurrentOctaveEntryReference = nullptr;
    // the first note in the voice will become the initial reference
  }

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
    fLogOutputStream <<
      "setCurrentOctaveEntryReferenceFromTheLilypondOptions()" <<
      ", octaveEntryKind is" <<
      lpsrOctaveEntryKindAsString (gLilypondOptions->fOctaveEntryKind) <<
      endl <<
      "Initial fCurrentOctaveEntryReference is ";

    if (fCurrentOctaveEntryReference) {
      fLogOutputStream <<
        "'" <<
        fCurrentOctaveEntryReference->asString () <<
        "'";
    }
    else {
      fLogOutputStream << "none";
    }
    fLogOutputStream << endl;
  }
#endif
}

//________________________________________________________________________
lpsr2LilypondTranslator::lpsr2LilypondTranslator (
  S_msrOptions&    msrOpts,
  S_lpsrOptions&   lpsrOpts,
  indentedOstream& logIOstream,
  indentedOstream& lilypondOutputStream,
  S_lpsrScore      lpsrScore)
    : fLogOutputStream (
        logIOstream),
      fLilypondCodeIOstream (
        lilypondOutputStream)
{
/* JMI
  segmentedLinesOstream
    testSegmentedLinesOstream (fLogOutputStream);

  fLogOutputStream <<
    "getAtEndOfSegment: " <<
    booleanAsString (
      testSegmentedLinesOstream.getAtEndOfSegment ()) <<
    endl;

  testSegmentedLinesOstream.setAtEndOfSegment (true);

  fLogOutputStream <<
    "getAtEndOfSegment: " <<
    booleanAsString (
      testSegmentedLinesOstream.getAtEndOfSegment ()) <<
    endl;

  testSegmentedLinesOstream <<
    "FOO" << endl; // <<
 //   endline;

  testSegmentedLinesOstream.getIndentedOstream () << flush;
  */

  fMsrOptions  = msrOpts;
  fLpsrOptions = lpsrOpts;

  // the LPSR score we're visiting
  fVisitedLpsrScore = lpsrScore;

  // inhibit the browsing of measures repeats replicas,
  // since Lilypond only needs the repeat measure
  fVisitedLpsrScore->
    getMsrScore ()->
      setInhibitMeasuresRepeatReplicasBrowsing ();

/* JMI
  // inhibit the browsing of measures repeat replicas,
  // since Lilypond only needs the measure number
  fVisitedLpsrScore->
    getMsrScore ()->
      setInhibitRestMeasuresBrowsing ();
*/

  // header handling
  fOnGoingHeader = false;

  // time
  fVoiceIsCurrentlySenzaMisura = false;
  fOnGoingVoiceCadenza = false;

  // staves
  fOnGoingStaff = false;

  // voices
  fOnGoingVoice = false;

  // octaves entry
  // ------------------------------------------------------
  /* initialize reference is:
        mobile in relative mode
        unused in absolute mode
        fixed  in fixed mode
  */
  switch (gLilypondOptions->fOctaveEntryKind) {
    case kOctaveEntryRelative:
      setCurrentOctaveEntryReferenceFromTheLilypondOptions ();
      break;

    case kOctaveEntryAbsolute:
      fCurrentOctaveEntryReference = nullptr;
      break;

    case kOctaveEntryFixed:
      // sanity check
      msrAssert (
        gLilypondOptions->fFixedOctaveEntrySemiTonesPitchAndOctave != nullptr,
        "gLilypondOptions->fFixedOctaveEntrySemiTonesPitchAndOctave is null");

      fCurrentOctaveEntryReference =
        msrNote::createNoteFromSemiTonesPitchAndOctave (
          K_NO_INPUT_LINE_NUMBER,
          gLilypondOptions->fFixedOctaveEntrySemiTonesPitchAndOctave);
      break;
  } // switch

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
    fLogOutputStream <<
      "lpsr2LilypondTranslator()" <<
      ", octaveEntryKind is" <<
      lpsrOctaveEntryKindAsString (gLilypondOptions->fOctaveEntryKind) <<
      endl <<
      "Initial fCurrentOctaveEntryReference is ";

    if (fCurrentOctaveEntryReference) {
      fLogOutputStream <<
        "'" <<
        fCurrentOctaveEntryReference->asString () <<
        "'";
    }
    else {
      fLogOutputStream << "none";
    }
    fLogOutputStream << endl;
  }
#endif

  // harmonies
  fOnGoingHarmonyVoice = false;
  fPowerChordHaveAlreadyBeenGenerated = false;

  // figured bass
  fOnGoingFiguredBassVoice = false;
  fCurrentFiguredBassFiguresCounter = 0;

  // repeats

  // rest measures
  fRemainingRestMeasuresNumber = 0;
  fOnGoingRestMeasures = false;

  // measures
  fCurrentVoiceMeasuresCounter = -1;

  // durations
  fLastMetWholeNotes = rational (0, 1);

  // notes
  fCurrentNotePrinObjectKind = kPrintObjectYes; // default value
  fOnGoingNote = false;

  // grace notes
  fOnGoingGraceNotesGroup = false;

  // articulations
  fCurrentArpeggioDirectionKind = kDirectionNone;

  // stems
  fCurrentStemKind = msrStem::kStemNone;

  // double tremolos

  // chords
  fOnGoingChord = false;

  // trills
  fOnGoingTrillSpanner = false;

  // spanners
  fCurrentSpannerPlacementKind = kPlacementNone;

  // stanzas
  fGenerateCodeForOngoingNonEmptyStanza = false;

  // score blocks
  fOnGoingScoreBlock = false;

  // part group blocks
  fNumberOfPartGroupBlocks = -1;
  fPartGroupBlocksCounter  = 0;

  // part blocks
  fNumberOfPartGroupBlockElements = -1;
  fPartGroupBlockElementsCounter  = 0;

  // staff blocks
  fNumberOfStaffBlocksElements = -1;
  fStaffBlocksCounter  = 0;
};

lpsr2LilypondTranslator::~lpsr2LilypondTranslator ()
{}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateLilypondCodeFromLpsrScore ()
{
  if (fVisitedLpsrScore) {
    // browse a msrScore browser
    msrBrowser<lpsrScore> browser (this);
    browser.browse (*fVisitedLpsrScore);
  }
}

//________________________________________________________________________
string lpsr2LilypondTranslator::absoluteOctaveAsLilypondString (
  int absoluteOctave)
{
  string result;

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotes) {
    fLilypondCodeIOstream <<
      endl <<
      "%{ absoluteOctave = " << absoluteOctave << " %} " <<
      endl;
  }
#endif

  // generate LilyPond absolute octave
  switch (absoluteOctave) {
    case 0:
      result = ",,,";
      break;
    case 1:
      result = ",,";
      break;
    case 2:
      result = ",";
      break;
    case 3:
      result = "";
      break;
    case 4:
      result = "'";
      break;
    case 5:
      result = "''";
      break;
    case 6:
      result = "'''";
      break;
    case 7:
      result = "''''";
      break;
    case 8:
      result = "'''''";
      break;
    default:
      {
        /* JMI
        stringstream s;

        s <<
          "%{absolute octave " << absoluteOctave << "???%}";

        result = s.str ();
        */
      }
  } // switch

  return result;
}

//________________________________________________________________________
string lpsr2LilypondTranslator::alterationKindAsLilypondString (
  msrAlterationKind alterationKind)
{
  string result;

  switch (alterationKind) {
    case kTripleFlat:
      result = "TRIPLE-FLAT";
      break;
    case kDoubleFlat:
      result = "DOUBLE-FLAT";
      break;
    case kSesquiFlat:
      result = "THREE-Q-FLAT";
      break;
    case kFlat:
      result = "FLAT";
      break;
    case kSemiFlat:
      result = "SEMI-FLAT";
      break;
    case kNatural:
      result = "NATURAL";
      break;
    case kSemiSharp:
      result = "SEMI-SHARP";
      break;
    case kSharp:
      result = "SHARP";
      break;
    case kSesquiSharp:
      result = "THREE-Q-SHARP";
      break;
    case kDoubleSharp:
      result = "DOUBLE-SHARP";
      break;
    case kTripleSharp:
      result = "TRIPLE-SHARP";
      break;
    case k_NoAlteration:
      result = "alteration???";
      break;
  } // switch

  return result;
}

//________________________________________________________________________
string lpsr2LilypondTranslator::lilypondOctaveInRelativeEntryMode (
  S_msrNote note)
{
  int inputLineNumber =
    note->getInputLineNumber ();

  // generate LilyPond octave relative to fCurrentOctaveEntryReference

  // in MusicXML, octave number is 4 for the octave starting with middle C
  int noteAbsoluteOctave =
    note->getNoteOctave ();

  msrDiatonicPitchKind
    noteDiatonicPitchKind =
      note->
        noteDiatonicPitchKind (
          inputLineNumber);

  msrDiatonicPitchKind
    referenceDiatonicPitchKind =
      fCurrentOctaveEntryReference->
        noteDiatonicPitchKind (
          inputLineNumber);

  string
    referenceDiatonicPitchKindAsString =
      fCurrentOctaveEntryReference->
        noteDiatonicPitchKindAsString (
          inputLineNumber);

  int
    referenceAbsoluteOctave =
      fCurrentOctaveEntryReference->
        getNoteOctave ();

  /*
    If no octave changing mark is used on a pitch, its octave is calculated
    so that the interval with the previous note is less than a fifth.
    This interval is determined without considering accidentals.
  */

  int
    noteAboluteDiatonicOrdinal =
      noteAbsoluteOctave * 7
        +
      noteDiatonicPitchKind - kC,

    referenceAboluteDiatonicOrdinal =
      referenceAbsoluteOctave * 7
        +
      referenceDiatonicPitchKind - kC;

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
    const int fieldWidth = 28;

    fLogOutputStream << left <<
      "lilypondOctaveInRelativeEntryMode() 1" <<
      endl <<

      setw (fieldWidth) <<
      "% noteAboluteDiatonicOrdinal" <<
      " = " <<
      noteAboluteDiatonicOrdinal <<
      endl <<

      setw (fieldWidth) <<
      "% referenceDiatonicPitchAsString" <<
      " = " <<
      referenceDiatonicPitchKindAsString <<
      endl <<
      setw (fieldWidth) <<
      "% referenceAbsoluteOctave" <<
       " = " <<
      referenceAbsoluteOctave <<
      endl <<
      setw (fieldWidth) <<
      "% referenceAboluteDiatonicOrdinal" <<
      " = " <<
      referenceAboluteDiatonicOrdinal <<
      endl <<
      endl;
  }
#endif

  stringstream s;

  // generate the octaves as needed
  if (noteAboluteDiatonicOrdinal >= referenceAboluteDiatonicOrdinal) {
    noteAboluteDiatonicOrdinal -= 4;
    while (noteAboluteDiatonicOrdinal >= referenceAboluteDiatonicOrdinal) {
      s << "'";
      noteAboluteDiatonicOrdinal -= 7;
    } // while
  }

  else {
    noteAboluteDiatonicOrdinal += 4;
    while (noteAboluteDiatonicOrdinal <= referenceAboluteDiatonicOrdinal) {
      s << ",";
      noteAboluteDiatonicOrdinal += 7;
    } // while
  }

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
    fLogOutputStream <<
      "lilypondOctaveInRelativeEntryMode() 2" <<
      ", result = " << s.str () <<
      endl <<
      endl;
  }
#endif

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::lilypondOctaveInFixedEntryMode (
  S_msrNote note)
{
  int inputLineNumber =
    note->getInputLineNumber ();

  // generate LilyPond octave relative to fCurrentOctaveEntryReference

  // in MusicXML, octave number is 4 for the octave starting with middle C

  /*
    Pitches in fixed octave entry mode only need commas or quotes
    when they are above or below fCurrentOctaveEntryReference,
    hence when their octave is different that the latter's.
  */

  int
    noteAbsoluteOctave =
      note->getNoteOctave ();

  int
    referenceAbsoluteOctave =
      fCurrentOctaveEntryReference->
        getNoteOctave ();

  int absoluteOctavesDifference =
    noteAbsoluteOctave - referenceAbsoluteOctave;

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
    fLogOutputStream << left <<
      "% noteAbsoluteOctave = " <<
      noteAbsoluteOctave <<
      ", referenceAbsoluteOctave = " <<
      referenceAbsoluteOctave <<
      ", refabsoluteOctavesDifferenceerenceAbsoluteOctave = " <<
      absoluteOctavesDifference <<
      endl;
  }
#endif

  stringstream s;

  // generate the octaves as needed
  switch (absoluteOctavesDifference) {
    case -12:
      s << ",,,,,,,,,,,,";
      break;
    case -11:
      s << ",,,,,,,,,,,";
      break;
    case -10:
      s << ",,,,,,,,,,";
      break;
    case -9:
      s << ",,,,,,,,,";
      break;
    case -8:
      s << ",,,,,,,,";
      break;
    case -7:
      s << ",,,,,,,";
      break;
    case -6:
      s << ",,,,,,";
      break;
    case -5:
      s << ",,,,,";
      break;
    case -4:
      s << ",,,,";
      break;
    case -3:
      s << ",,,";
      break;
    case -2:
      s << ",,";
      break;
    case -1:
      s << ",";
      break;
    case 0:
      break;
    case 1:
      s << "'";
      break;
    case 2:
      s << "''";
      break;
    case 3:
      s << "'''";
      break;
    case 4:
      s << "''''";
      break;
    case 5:
      s << "'''''";
      break;
    case 6:
      s << "''''''";
      break;
    case 7:
      s << "'''''''";
      break;
    case 8:
      s << "''''''''";
      break;
    case 9:
      s << "'''''''''";
      break;
    case 10:
      s << "''''''''''";
      break;
    case 11:
      s << "'''''''''''";
      break;
    case 12:
      s << "''''''''''''";
      break;
    default:
      s << "!!!";
  } // switch

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::stringTuningAsLilypondString (
  int               inputLineNumber,
  S_msrStringTuning stringTuning)
{
  msrDiatonicPitchKind
    stringTuningDiatonicPitchKind =
      stringTuning->
        getStringTuningDiatonicPitchKind ();

  msrAlterationKind
    stringTuningAlterationKind =
      stringTuning->
       getStringTuningAlterationKind ();

  int
    stringTuningOctave =
      stringTuning->
       getStringTuningOctave ();

  // compute the quartertones pitch
  msrQuarterTonesPitchKind
    quarterTonesPitchKind =
      quarterTonesPitchKindFromDiatonicPitchAndAlteration (
        inputLineNumber,
        stringTuningDiatonicPitchKind,
        stringTuningAlterationKind);

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceScordaturas) {
    int
      getStringTuningNumber =
        stringTuning->
          getStringTuningNumber ();

    fLogOutputStream <<
      endl <<
      "%getStringTuningNumber = " <<
      getStringTuningNumber <<
      endl <<
      "%stringTuningDiatonicPitchKind = " <<
      msrDiatonicPitchKindAsString (
        stringTuningDiatonicPitchKind) <<
      endl <<
      "%stringTuningAlterationKind = " <<
      alterationKindAsLilypondString (
        stringTuningAlterationKind) <<
      endl <<
      "%stringTuningOctave = " <<
      stringTuningOctave <<
      endl <<
      "%quarterTonesPitchKind = " <<
      quarterTonesPitchKind <<
      endl <<
      "%quarterTonesPitchKindAsString: " <<
      msrQuarterTonesPitchKindAsString (
        gLpsrOptions->
          fLpsrQuarterTonesPitchesLanguageKind,
          quarterTonesPitchKind) <<
      endl <<
      endl;
  }
#endif

  stringstream s;

  s <<
    msrQuarterTonesPitchKindAsString (
      gLpsrOptions->
        fLpsrQuarterTonesPitchesLanguageKind,
        quarterTonesPitchKind) <<
    absoluteOctaveAsLilypondString (
      stringTuningOctave);

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::notePitchAsLilypondString (
  S_msrNote note)
{
  stringstream s;

  // should an editorial accidental be generated?
  switch (note->getNoteEditorialAccidentalKind ()) {
    case msrNote::kNoteEditorialAccidentalYes:
      s <<
        "\\editorialAccidental ";
      break;
    case msrNote::kNoteEditorialAccidentalNo:
      break;
  } // switch

  // get the note quarter tones pitch
  msrQuarterTonesPitchKind
    noteQuarterTonesPitchKind =
      note->
        getNoteQuarterTonesPitchKind ();

  // fetch the quarter tones pitch as string
  string
    quarterTonesPitchKindAsString =
      msrQuarterTonesPitchKindAsString (
        gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
        noteQuarterTonesPitchKind);

  // get the note quarter tones display pitch
  msrQuarterTonesPitchKind
    noteQuarterTonesDisplayPitchKind =
      note->
        getNoteQuarterTonesDisplayPitchKind ();

  // fetch the quarter tones display pitch as string
  string
    quarterTonesDisplayPitchKindAsString =
      msrQuarterTonesPitchKindAsString (
        gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
        noteQuarterTonesDisplayPitchKind);

  // generate the pitch
  s <<
    quarterTonesPitchKindAsString;

  // in MusicXML, octave number is 4 for the octave
  // starting with middle C, LilyPond's c'
  int
    noteAbsoluteOctave =
      note->getNoteOctave ();

  // should an absolute octave be generated?
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
    int
      noteAbsoluteDisplayOctave =
        note->getNoteDisplayOctave ();

    const int fieldWidth = 39;

    fLogOutputStream << left <<
      "notePitchAsLilypondString() 1" <<
      endl <<

      setw (fieldWidth) <<
      "% quarterTonesPitchKindAsString" <<
      " = " <<
      quarterTonesPitchKindAsString <<
      endl <<
      setw (fieldWidth) <<
      "% quarterTonesDisplayPitchKindAsString" <<
      " = " <<
      quarterTonesDisplayPitchKindAsString <<
      endl <<

      setw (fieldWidth) <<
      "% noteAbsoluteOctave" <<
      " = " <<
      noteAbsoluteOctave <<
      endl <<
      setw (fieldWidth) <<
      "% noteAbsoluteDisplayOctave" <<
      " = " <<
      noteAbsoluteDisplayOctave <<
      endl <<

      setw (fieldWidth) <<
      "% line" <<
      " = " <<
      note->getInputLineNumber () <<
      endl;
  }
#endif

  switch (gLilypondOptions->fOctaveEntryKind) {
    case kOctaveEntryRelative:
      if (! fCurrentOctaveEntryReference) {
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
          fLogOutputStream <<
            "notePitchAsLilypondString() 2: fCurrentOctaveEntryReference is null" <<
            " upon note " << note->asString () <<
            ", line " << note->getInputLineNumber () <<
            endl;
        }
#endif

        // generate absolute octave
        s <<
          absoluteOctaveAsLilypondString (
            noteAbsoluteOctave);

        // fCurrentOctaveEntryReference will be set to note later
      }
      else {
#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
          fLogOutputStream <<
            "notePitchAsLilypondString() 3: fCurrentOctaveEntryReference is '" <<
            fCurrentOctaveEntryReference->asString () <<
            "' upon note " << note->asString () <<
            ", line " << note->getInputLineNumber () <<
            endl;
        }
#endif

        // generate octave relative to mobile fCurrentOctaveEntryReference
        s <<
          lilypondOctaveInRelativeEntryMode (note);
      }
      break;

    case kOctaveEntryAbsolute:
      // generate LilyPond absolute octave
      s <<
        absoluteOctaveAsLilypondString (
          noteAbsoluteOctave);
      break;

    case kOctaveEntryFixed:
      // generate octave relative to fixed fCurrentOctaveEntryReference
      s <<
        lilypondOctaveInFixedEntryMode (note);
      break;
  } // switch

  // should an accidental be generated? JMI this can be fine tuned with cautionary
  switch (note->getNoteAccidentalKind ()) {
    case msrNote::kNoteAccidentalNone:
      break;
    default:
      s <<
        "!";
      break;
  } // switch

  // should a cautionary accidental be generated?
  switch (note->getNoteCautionaryAccidentalKind ()) {
    case msrNote::kNoteCautionaryAccidentalYes:
      s <<
        "?";
      break;
    case msrNote::kNoteCautionaryAccidentalNo:
      break;
  } // switch

#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceNotesOctaveEntry || gTraceOptions->fTraceNotesDetails) {
          fLogOutputStream << endl;
        }
#endif

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::durationAsLilypondString (
  int      inputLineNumber,
  rational wholeNotes)
{
  string result;

  bool generateExplicitDuration;

  if (wholeNotes != fLastMetWholeNotes) {
    generateExplicitDuration = true;
    fLastMetWholeNotes = wholeNotes;
  }
  else {
    generateExplicitDuration =
      gLilypondOptions->fAllDurations;
  }

  if (generateExplicitDuration) {
    result =
      wholeNotesAsLilypondString (
        inputLineNumber,
        wholeNotes);
  }

  return result;
}

//________________________________________________________________________
string lpsr2LilypondTranslator::pitchedRestAsLilypondString (
  S_msrNote note)
{
  stringstream s;

  // get the note quarter tones pitch
  msrQuarterTonesPitchKind
    noteQuarterTonesPitchKind =
      note->
        getNoteQuarterTonesPitchKind ();

  // fetch the quarter tones pitch as string
  string
    noteQuarterTonesPitchKindAsString =
      msrQuarterTonesPitchKindAsString (
        gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
        noteQuarterTonesPitchKind);

  // get the note quarter tones display pitch
  msrQuarterTonesPitchKind
    noteQuarterTonesDisplayPitchKind =
      note->
        getNoteQuarterTonesDisplayPitchKind ();

  // fetch the quarter tones display pitch as string
  string
    quarterTonesDisplayPitchKindAsString =
      msrQuarterTonesPitchKindAsString (
        gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
        noteQuarterTonesDisplayPitchKind);

  // generate the display pitch
  s <<
    note->
      noteDisplayPitchKindAsString ();
//    note->notePitchAsString (); JMI
//    quarterTonesDisplayPitchAsString;

  // should an absolute octave be generated?
  int
    noteAbsoluteDisplayOctave =
      note->getNoteDisplayOctave ();

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotes) {
    // in MusicXML, octave number is 4 for the octave starting with middle C
    int noteAbsoluteOctave =
      note->getNoteOctave ();

    const int fieldWidth = 28;

    fLogOutputStream << left <<
      "pitchedRestAsLilypondString()" <<
      endl <<

      setw (fieldWidth) <<
      "% noteQuarterTonesPitchKindAsString" <<
      " = " <<
      noteQuarterTonesPitchKindAsString <<
      endl <<
      setw (fieldWidth) <<
      "% quarterTonesDisplayPitch" <<
      " = " <<
      quarterTonesDisplayPitchKindAsString <<
      endl <<

      setw (fieldWidth) <<
      "% noteAbsoluteOctave" <<
      " = " <<
      noteAbsoluteOctave <<
      endl <<
      setw (fieldWidth) <<
      "% noteAbsoluteDisplayOctave" <<
      " = " <<
      noteAbsoluteDisplayOctave <<
      endl <<

      setw (fieldWidth) <<
      "% line" <<
      " = " <<
      note->getInputLineNumber () <<
      endl;
  }
#endif

  switch (gLilypondOptions->fOctaveEntryKind) {
    case kOctaveEntryRelative:
    // generate LilyPond octave relative to fCurrentOctaveEntryReference
    s <<
      lilypondOctaveInRelativeEntryMode (note);
      break;
    case kOctaveEntryAbsolute:
      // generate LilyPond absolute octave
      s <<
        absoluteOctaveAsLilypondString (
          noteAbsoluteDisplayOctave);
      break;
    case kOctaveEntryFixed:
      // generate LilyPond octave relative to fCurrentOctaveEntryReference
      s <<
        lilypondOctaveInFixedEntryMode (note);
      break;
  } // switch

  // generate the skip duration
  s <<
    durationAsLilypondString (
      note->getInputLineNumber (),
      note->
        getNoteSoundingWholeNotes ());

  // generate the '\rest'
  s <<
    "\\rest ";

  return s.str ();
}

void lpsr2LilypondTranslator::generateNoteHeadColor (
  S_msrNote note)
{
  int inputLineNumber =
    note->getInputLineNumber ();

  // note color, unofficial ??? JMI
  msrColor
    noteColor =
      note->getNoteColor ();

  if (! noteColor.isEmpty ()) {
    // generate code for color RGB
    string noteRGB = noteColor.getColorRGB ();

    if (noteRGB.size () == 6) {
      string
        noteR = noteRGB.substr (0, 2),
        noteG = noteRGB.substr (2, 2),
        noteB = noteRGB.substr (4, 2);

       // \once \override NoteHead.color = #(map (lambda (x) (/ x 255)) '(#X00 #X00 #XFF))

        fLilypondCodeIOstream <<
          "\\once \\override NoteHead.color = #(map (lambda (x) (/ x 255)) "
          "'(" <<
          "#X" << noteRGB [0] << noteRGB [1] <<
          " " <<
          "#X" << noteRGB [2] << noteRGB [3] <<
          " " <<
          "#X" << noteRGB [4] << noteRGB [5] <<
          "))" <<
          endl;
      }
    else {
      stringstream s;

      s <<
        "note RGB color '" <<
        noteRGB <<
        "' is ill-formed" <<
        ", line " << inputLineNumber;

      msrInternalError (
        gGeneralOptions->fInputSourceName,
        inputLineNumber,
        __FILE__, __LINE__,
        s.str ());
    }

    // ignore color alpha
  }
}

void lpsr2LilypondTranslator::generateNoteLigatures (
  S_msrNote note)
{
  list<S_msrLigature>
    noteLigatures =
      note->getNoteLigatures ();

  if (noteLigatures.size ()) {
    list<S_msrLigature>::const_iterator i;
    for (
      i=noteLigatures.begin ();
      i!=noteLigatures.end ();
      i++
    ) {
      S_msrLigature ligature = (*i);

      switch (ligature->getLigatureKind ()) {
        case msrLigature::kLigatureNone:
          break;

        case msrLigature::kLigatureStart:
          {
            /*
              the edge height is relative to the voice,
              i.e. a positive value points down in voices 1 and 3
              and it points up in voices 2 and 4,
            */

            // fetch note's voice
            S_msrVoice
              noteVoice =
                note->
                  getNoteMeasureUpLink ()->
                    getMeasureSegmentUpLink ()->
                      getSegmentVoiceUpLink ();

            // determine vertical flipping factor
            int ligatureVerticalFlippingFactor = 0;

            switch (noteVoice->getRegularVoiceStaffSequentialNumber ()) {
              case 1:
              case 3:
                ligatureVerticalFlippingFactor = 1;
                break;
              case 2:
              case 4:
                ligatureVerticalFlippingFactor = -1;
                break;
              default:
                ;
            } // switch

#ifdef TRACE_OPTIONS
            if (gTraceOptions->fTraceLigatures) {
              fLogOutputStream <<
                "Ligature vertical flipping factore for note '" <<
                note->asString () <<
                "' in voice \"" <<
                noteVoice->getVoiceName () <<
                "\" is " <<
                ligatureVerticalFlippingFactor <<
                ", line " << ligature->getInputLineNumber () <<
                endl;
            }
#endif

            // compute ligature start edge height
            const float edgeHeightAbsValue = 0.75;

            float       ligatureStartEdgeHeight = 0.0;

            switch (ligature->getLigatureLineEndKind ()) {
              case msrLigature::kLigatureLineEndUp:
                ligatureStartEdgeHeight =
                  - ligatureVerticalFlippingFactor * edgeHeightAbsValue;
                break;

              case msrLigature::kLigatureLineEndDown:
                ligatureStartEdgeHeight =
                  ligatureVerticalFlippingFactor * edgeHeightAbsValue;
                break;

              case msrLigature::kLigatureLineEndBoth: // JMI
                ligatureStartEdgeHeight =
                  - ligatureVerticalFlippingFactor * edgeHeightAbsValue;
                break;

              case msrLigature::kLigatureLineEndArrow: // JMI
                fLilypondCodeIOstream <<
                  "%{ligatureLineEndArrow???%} ";
                break;

              case msrLigature::kLigatureLineEndNone:
                ligatureStartEdgeHeight = 0;
                break;
            } // switch

            // fetch ligature's other end
            S_msrLigature
              ligatureOtherEnd =
                ligature->
                  getLigatureOtherEndSideLink ();

            // compute ligature end edge height
            float ligatureEndEdgeHeight = 0.0;

            switch (ligatureOtherEnd->getLigatureLineEndKind ()) {
              case msrLigature::kLigatureLineEndUp:
                ligatureEndEdgeHeight =
                  - ligatureVerticalFlippingFactor * edgeHeightAbsValue;
                break;

              case msrLigature::kLigatureLineEndDown:
                ligatureEndEdgeHeight =
                  ligatureVerticalFlippingFactor * edgeHeightAbsValue;
                break;

              case msrLigature::kLigatureLineEndBoth: // JMI
                ligatureEndEdgeHeight =
                  - ligatureVerticalFlippingFactor * edgeHeightAbsValue;
                break;

              case msrLigature::kLigatureLineEndArrow: // JMI
                fLilypondCodeIOstream <<
                  "%{ligatureLineEndArrow???%} ";
                break;

              case msrLigature::kLigatureLineEndNone:
                ligatureEndEdgeHeight = 0;
                break;
            } // switch

            // generate the code the the edge-height pair of values
            fLilypondCodeIOstream <<
              endl <<
              "\\once \\override Staff.LigatureBracket.edge-height = #'(" <<
              setprecision (2) <<
              ligatureStartEdgeHeight <<
              " . " <<
              setprecision (2) <<
              ligatureEndEdgeHeight <<
              ")" <<
              " %{ " <<
              ligature->getInputLineNumber () <<
              " %}" <<
              endl;
          }

          // generate ligature line type if any
          switch (ligature->getLigatureLineTypeKind ()) {
            case kLineTypeSolid:
              break;
            case kLineTypeDashed:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override LigatureBracket.style = #'dashed-line" <<
                endl;
              break;
            case kLineTypeDotted:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override LigatureBracket.style = #'dotted-line" <<
                endl;
              break;
            case kLineTypeWavy:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override LigatureBracket.style = #'zigzag" <<
                endl;
              break;
          } // switch

          fLilypondCodeIOstream << "\\[ ";
          break;

        case msrLigature::kLigatureContinue:
          break;

        case msrLigature::kLigatureStop:
   // JMI       fLilypondCodeIOstream << "\\] ";
          break;
      } // switch
    } // for
  }
}

void lpsr2LilypondTranslator::generateNoteStem (
  S_msrNote note)
{
  S_msrStem
    noteStem =
      note->getNoteStem ();

  if (noteStem) {
    msrStem::msrStemKind noteStemKind;

    if (noteStem) {
      noteStemKind = noteStem->getStemKind ();
    }
    else {
      noteStemKind = msrStem::kStemNone;
    }

    // handle note stem before printing note itself
    switch (note->getNoteKind ()) {

      case msrNote::k_NoNoteKind:
        break;

      case msrNote::kRestNote:
        break;

      case msrNote::kSkipNote:
        break;

      case msrNote::kUnpitchedNote:
        {
          // is this note in a tab staff?
          S_msrStaff
            noteStaff =
              note->
                getNoteMeasureUpLink ()->
                  getMeasureSegmentUpLink ()->
                    getSegmentVoiceUpLink ()->
                      getVoiceStaffUpLink ();

          switch (noteStaff->getStaffKind ()) {
            case msrStaff::kStaffTablature:
              break;
            case msrStaff::kStaffHarmony:
              break;
            case msrStaff::kStaffFiguredBass:
              break;
            case msrStaff::kStaffDrum:
              break;
            case msrStaff::kStaffRythmic: // JMI
              // should the stem be omitted?
              if (note->getNoteIsStemless ()) {
                fLilypondCodeIOstream <<
                  endl <<
                  "\\stemNeutral "; // JMI ""\\once\\omit Stem ";

                fCurrentStemKind = msrStem::kStemNone;
              }

              // should stem direction be generated?
              if (noteStemKind != fCurrentStemKind) {
                switch (noteStemKind) {
                  case msrStem::kStemNone:
                    fLilypondCodeIOstream << "\\stemNeutral ";
                    break;
                  case msrStem::kStemUp:
                    fLilypondCodeIOstream << "\\stemUp ";
                    break;
                  case msrStem::kStemDown:
                    fLilypondCodeIOstream << "\\stemDown ";
                    break;
                  case msrStem::kStemDouble: // JMI ???
                    break;
                } // switch

                fCurrentStemKind = noteStemKind;
              }
              break;

            case msrStaff::kStaffRegular:
              // should the stem be omitted?
              if (note->getNoteIsStemless ()) {
                fLilypondCodeIOstream <<
                  endl <<
                  "\\stemNeutral "; // JMI ""\\once\\omit Stem ";

                fCurrentStemKind = msrStem::kStemNone;
              }

              // should stem direction be generated?
              if (gLilypondOptions->fStems) {
                if (noteStemKind != fCurrentStemKind) {
                  switch (noteStemKind) {
                    case msrStem::kStemNone:
                      fLilypondCodeIOstream << "\\stemNeutral ";
                      break;
                    case msrStem::kStemUp:
                      fLilypondCodeIOstream << "\\stemUp ";
                      break;
                    case msrStem::kStemDown:
                      fLilypondCodeIOstream << "\\stemDown ";
                      break;
                    case msrStem::kStemDouble: // JMI ???
                      break;
                  } // switch

                  fCurrentStemKind = noteStemKind;
                }
              }
              break;
          } // switch
        }
        break;

      case msrNote::kStandaloneNote:
        {
          // is this note in a tab staff?
          S_msrStaff
            noteStaff =
              note->
                getNoteMeasureUpLink ()->
                  getMeasureSegmentUpLink ()->
                    getSegmentVoiceUpLink ()->
                      getVoiceStaffUpLink ();

          switch (noteStaff->getStaffKind ()) {
            case msrStaff::kStaffTablature:
              break;
            case msrStaff::kStaffHarmony:
              break;
            case msrStaff::kStaffFiguredBass:
              break;
            case msrStaff::kStaffDrum:
              break;
            case msrStaff::kStaffRythmic:
              break;

            case msrStaff::kStaffRegular:
              // should the stem be omitted?
              if (note->getNoteIsStemless ()) {
                fLilypondCodeIOstream <<
                  endl <<
                  "\\stemNeutral "; // JMI ""\\once\\omit Stem ";

                fCurrentStemKind = msrStem::kStemNone;
              }

              // should stem direction be generated?
              if (gLilypondOptions->fStems) {
                if (noteStemKind != fCurrentStemKind) {
                  switch (noteStemKind) {
                    case msrStem::kStemNone:
                      fLilypondCodeIOstream << "\\stemNeutral ";
                      break;
                    case msrStem::kStemUp:
                      fLilypondCodeIOstream << "\\stemUp ";
                      break;
                    case msrStem::kStemDown:
                      fLilypondCodeIOstream << "\\stemDown ";
                      break;
                    case msrStem::kStemDouble: // JMI ???
                      break;
                  } // switch

                  fCurrentStemKind = noteStemKind;
                }
              }
              break;
          } // switch
        }
        break;

      case msrNote::kDoubleTremoloMemberNote:
        {
          // should the stem be omitted?
          if (note->getNoteIsStemless ()) {
            fLilypondCodeIOstream <<
              endl <<
              "\\stemNeutral"; // JMI ""\\once\\omit Stem ";
          }

          // should stem direction be generated?
          if (gLilypondOptions->fStems) {
            if (noteStemKind != fCurrentStemKind) {
              switch (noteStemKind) {
                case msrStem::kStemNone:
                  fLilypondCodeIOstream << "\\stemNeutral ";
                  break;
                case msrStem::kStemUp:
                  fLilypondCodeIOstream << "\\stemUp ";
                  break;
                case msrStem::kStemDown:
                  fLilypondCodeIOstream << "\\stemDown ";
                  break;
                  /* JMI
                case msrStem::kStemNone:
                  break;
                  */
                case msrStem::kStemDouble: // JMI ???
                  break;
              } // switch

              fCurrentStemKind = noteStemKind;
            }
          }
        }
        break;

      case msrNote::kGraceNote:
      case msrNote::kGraceChordMemberNote:
        break;

      case msrNote::kChordMemberNote:
       // don't omit stems for chord member note JMI
       break;

      case msrNote::kTupletMemberNote:
      case msrNote::kGraceTupletMemberNote:
      case msrNote::kTupletMemberUnpitchedNote:
        break;
    } // switch
  }
}

void lpsr2LilypondTranslator::generateNoteHead (
  S_msrNote note)
{
  if (! note->getNoteIsARest ()) { // JMI ???
    msrNote::msrNoteHeadKind
      noteHeadKind =
        note->getNoteHeadKind ();

    // these tweaks should occur right before the note itself
    switch (noteHeadKind) {
      case msrNote::kNoteHeadSlash:
        fLilypondCodeIOstream << "\\tweak style #'slash ";
        break;
      case msrNote::kNoteHeadTriangle:
        fLilypondCodeIOstream << "\\tweak style #'triangle ";
        break;
      case msrNote::kNoteHeadDiamond:
   // JMI     fLilypondCodeIOstream << "\\tweak style #'diamond ";
        fLilypondCodeIOstream << "\\harmonic ";
        break;
      case msrNote::kNoteHeadSquare:
        fLilypondCodeIOstream << "\\tweak style #'la ";
        break;
      case msrNote::kNoteHeadCross:
        fLilypondCodeIOstream << "\\tweak style #'cross ";
        break;
      case msrNote::kNoteHeadX:
        fLilypondCodeIOstream << "\\tweak style #'cross %{x%} ";
        break;
      case msrNote::kNoteHeadCircleX:
        fLilypondCodeIOstream << "\\tweak style #'xcircle ";
        break;
      case msrNote::kNoteHeadInvertedTriangle:
        fLilypondCodeIOstream << "%{kNoteHeadInvertedTriangle%} ";
        break;
      case msrNote::kNoteHeadArrowDown:
        fLilypondCodeIOstream << "%{kNoteHeadArrowDown%} ";
        break;
      case msrNote::kNoteHeadArrowUp:
        fLilypondCodeIOstream << "%{kNoteHeadArrowUp%} ";
        break;
      case msrNote::kNoteHeadSlashed:
        fLilypondCodeIOstream << "%{kNoteHeadSlashed%} ";
        break;
      case msrNote::kNoteHeadBackSlashed:
        fLilypondCodeIOstream << "%{kNoteHeadBackSlashed%} ";
        break;
      case msrNote::kNoteHeadNormal:
   // JMI     fLilypondCodeIOstream << "%{kNoteHeadNormal%} ";
        break;
      case msrNote::kNoteHeadCluster:
        fLilypondCodeIOstream << "%{kNoteHeadCluster%} ";
        break;
      case msrNote::kNoteHeadCircleDot:
        fLilypondCodeIOstream << "%{kNoteHeadCircleDot%} ";
        break;
      case msrNote::kNoteHeadLeftTriangle:
        fLilypondCodeIOstream << "%{kNoteHeadLeftTriangle%} ";
        break;
      case msrNote::kNoteHeadRectangle:
        fLilypondCodeIOstream << "%{kNoteHeadRectangle%} ";
        break;
      case msrNote::kNoteHeadNone:
        fLilypondCodeIOstream << "\\once\\omit NoteHead ";
        break;
      case msrNote::kNoteHeadDo:
        fLilypondCodeIOstream << "\\tweak style #'do ";
        break;
      case msrNote::kNoteHeadRe:
        fLilypondCodeIOstream << "\\tweak style #'re ";
        break;
      case msrNote::kNoteHeadMi:
        fLilypondCodeIOstream << "\\tweak style #'mi ";
        break;
      case msrNote::kNoteHeadFa:
        fLilypondCodeIOstream << "\\tweak style #'fa ";
        break;
      case msrNote::kNoteHeadFaUp:
        fLilypondCodeIOstream << "\\tweak style #'triangle ";
        break;
      case msrNote::kNoteHeadSo:
        fLilypondCodeIOstream << "\\tweak style #'sol ";
        break;
      case msrNote::kNoteHeadLa:
        fLilypondCodeIOstream << "\\tweak style #'la ";
        break;
      case msrNote::kNoteHeadTi:
        fLilypondCodeIOstream << "\\tweak style #'ti ";
        break;
    } // switch
  }
}

void lpsr2LilypondTranslator::generateCodeBeforeNote (
  S_msrNote note)
{
  // print the note head color
  generateNoteHeadColor (note);

  // print the note ligatures if any
  list<S_msrLigature>
    noteLigatures =
      note->getNoteLigatures ();

  if (noteLigatures.size ()) {
    generateNoteLigatures (note);
  }

  // print note stem kind
  S_msrStem
    noteStem =
      note->getNoteStem ();

  if (noteStem) {
    generateNoteStem (note);
  }

  // handling note head
  generateNoteHead (note);
}

void lpsr2LilypondTranslator::generateCodeForNote (
  S_msrNote note)
{
  int inputLineNumber =
    note->getInputLineNumber ();

  ////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////
  // print the note itself
  ////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////

  switch (note->getNoteKind ()) {

    case msrNote::k_NoNoteKind:
      break;

    case msrNote::kRestNote:
      {
        // get pitched rest status
        bool noteIsAPitchedRest =
          note->noteIsAPitchedRest ();

        if (noteIsAPitchedRest) {
          // pitched rest
          fLilypondCodeIOstream <<
            pitchedRestAsLilypondString (note);

          // this note is the new relative octave reference
          // (the display quarter tone pitch and octave
          // have been copied to the note octave
          // in the msrNote::msrNote () constructor,
          // since the note octave is used in relative code generation)
          switch (gLilypondOptions->fOctaveEntryKind) {
            case kOctaveEntryRelative:
              fCurrentOctaveEntryReference = note;
              break;
            case kOctaveEntryAbsolute:
              break;
            case kOctaveEntryFixed:
              break;
          } // switch
        }

        else {
          // unpitched rest
          // get the note sounding whole notes
          rational
            noteSoundingWholeNotes =
              note->getNoteSoundingWholeNotes ();

          // print the rest name and duration
          if (note->getNoteOccupiesAFullMeasure ()) {
            fLilypondCodeIOstream <<
              "R" <<
              /* JMI
              restMeasuresWholeNoteAsLilypondString (
                inputLineNumber,
                noteSoundingWholeNotes);
                */
              wholeNotesAsLilypondString (
                inputLineNumber,
                noteSoundingWholeNotes);
          }

          else {
            fLilypondCodeIOstream <<
              "r" <<
              durationAsLilypondString (
                inputLineNumber,
                noteSoundingWholeNotes);

/* JMI BOF
            if (fOnGoingVoiceCadenza) { // JMI
              if (noteSoundingWholeNotes != rational (1, 1)) {
                / * JMI
                // force the generation of the duration if needed
                if (! gLilypondOptions->fAllDurations) {
                  fLilypondCodeIOstream << // JMI
                    wholeNotesAsLilypondString (
                      inputLineNumber,
                      noteSoundingWholeNotes);
                }
                * /

                // generate the multiplying factor
                fLilypondCodeIOstream << // JMI
                  "*" <<
                  noteSoundingWholeNotes <<
                  "";
              }
            }
                  */
          }

          // an unpitched rest is no relative octave reference,
          // the preceding one is kept
        }
      }
      break;

    case msrNote::kSkipNote:
      // print the skip name
      fLilypondCodeIOstream << "s";

      // print the skip duration
      fLilypondCodeIOstream <<
        durationAsLilypondString (
          inputLineNumber,
          note->
            getNoteSoundingWholeNotes ());

      // a rest is no relative octave reference,
      // the preceding one is kept
      break;

    case msrNote::kUnpitchedNote:
      {
        // print the note name, "e" by convention
        fLilypondCodeIOstream <<
            "e";

        rational
          noteSoundingWholeNotes =
            note->
              getNoteSoundingWholeNotes ();

        // print the note duration
        fLilypondCodeIOstream <<
          durationAsLilypondString (
            inputLineNumber,
            noteSoundingWholeNotes);

        // handle delayed ornaments if any
        if (note->getNoteDelayedTurnOrnament ()) {
          // c2*2/3 ( s2*1/3\turn) JMI
          // we need the explicit duration in all cases,
          // regardless of gGeneralOptions->fAllDurations
          fLilypondCodeIOstream <<
            wholeNotesAsLilypondString (
              inputLineNumber,
              noteSoundingWholeNotes) <<
            "*" <<
            gLilypondOptions->fDelayedOrnamentsFraction;
        }

/* JMI
        // print the tie if any
        {
          S_msrTie noteTie = note->getNoteTie ();

          if (noteTie) {
            if (noteTie->getTieKind () == msrTie::kTieStart) {
              fLilypondCodeIOstream <<
                "%{line: " << inputLineNumber << "%}" <<
                " ~  %{kUnpitchedNote%}"; // JMI spaces???
            }
          }
        }
        */
      }
      break;

    case msrNote::kStandaloneNote:
      {
        // print the note name
        fLilypondCodeIOstream <<
          notePitchAsLilypondString (note);

        rational
          noteSoundingWholeNotes =
            note->
              getNoteSoundingWholeNotes ();

        // print the note duration
        fLilypondCodeIOstream <<
          durationAsLilypondString (
            inputLineNumber,
            noteSoundingWholeNotes);

        // handle delayed ornaments if any
        if (note->getNoteDelayedTurnOrnament ()) {
          // c2*2/3 ( s2*1/3\turn) JMI
          // we need the explicit duration in all cases,
          // regardless of gGeneralOptions->fAllDurations
          fLilypondCodeIOstream <<
          //* JMI TOO MUCH
            wholeNotesAsLilypondString (
              inputLineNumber,
              noteSoundingWholeNotes) <<
              //*/
            "*" <<
            gLilypondOptions->fDelayedOrnamentsFraction;
        }

        // print the tie if any
        {
          S_msrTie noteTie = note->getNoteTie ();

          if (noteTie) {
            if (noteTie->getTieKind () == msrTie::kTieStart) {
      //        fLilypondCodeIOstream << " ~ %{kStandaloneNote%}"; // JMI
            }
          }
        }

        // this note is the new relative octave reference
        switch (gLilypondOptions->fOctaveEntryKind) {
          case kOctaveEntryRelative:
            fCurrentOctaveEntryReference = note;
            break;
          case kOctaveEntryAbsolute:
            break;
          case kOctaveEntryFixed:
            break;
        } // switch
      }
      break;

    case msrNote::kDoubleTremoloMemberNote:
      // print the note name
      fLilypondCodeIOstream <<
        notePitchAsLilypondString (note);

      // print the note duration
      fLilypondCodeIOstream <<
        wholeNotesAsLilypondString (
          inputLineNumber,
          note->getNoteSoundingWholeNotes ());

      // handle delayed ornaments if any
      if (note->getNoteDelayedTurnOrnament ()) {
        // c2*2/3 ( s2*1/3\turn JMI
        fLilypondCodeIOstream <<
          "*" <<
          gLilypondOptions->fDelayedOrnamentsFraction;
      }

/* JMI
      // print the tie if any
      {
        S_msrTie noteTie = note->getNoteTie ();

        if (noteTie) {
          if (noteTie->getTieKind () == msrTie::kTieStart) {
            fLilypondCodeIOstream <<
              "%{line: " << inputLineNumber << "%}" <<
              " ~ %{kDoubleTremoloMemberNote%}";
          }
        }
      }
*/

      // this note is the new relative octave reference
      switch (gLilypondOptions->fOctaveEntryKind) {
        case kOctaveEntryRelative:
          fCurrentOctaveEntryReference = note;
          break;
        case kOctaveEntryAbsolute:
          break;
        case kOctaveEntryFixed:
          break;
      } // switch
      break;

    case msrNote::kGraceNote:
      // print the note name
      fLilypondCodeIOstream <<
        notePitchAsLilypondString (note);

      // print the grace note's graphic duration
      fLilypondCodeIOstream <<
        msrDurationKindAsString (
          note->
            getNoteGraphicDurationKind ());

      // print the dots if any JMI ???
      for (int i = 0; i < note->getNoteDotsNumber (); i++) {
        fLilypondCodeIOstream << ".";
      } // for

      // don't print the tie if any, 'acciacattura takes care of it
      /*
      {
        S_msrTie noteTie = note->getNoteTie ();

        if (noteTie) {
          if (noteTie->getTieKind () == msrTie::kTieStart) {
            fLilypondCodeIOstream <<
              "%{line: " << inputLineNumber << "%}" <<
              "~  %{kGraceNote%}";
          }
        }
      }
      */

      // this note is the new relative octave reference
      switch (gLilypondOptions->fOctaveEntryKind) {
        case kOctaveEntryRelative:
          fCurrentOctaveEntryReference = note;
          break;
        case kOctaveEntryAbsolute:
          break;
        case kOctaveEntryFixed:
          break;
      } // switch
      break;

    case msrNote::kGraceChordMemberNote:
      // print the note name
      fLilypondCodeIOstream <<
        notePitchAsLilypondString (note);

      // dont't print the grace note's graphic duration

      // print the dots if any JMI ???
      for (int i = 0; i < note->getNoteDotsNumber (); i++) {
        fLilypondCodeIOstream << ".";
      } // for

      // don't print the tie if any, 'acciacattura takes care of it
      /*
      {
        S_msrTie noteTie = note->getNoteTie ();

        if (noteTie) {
          if (noteTie->getTieKind () == msrTie::kTieStart) {
            fLilypondCodeIOstream <<
              "%{line: " << inputLineNumber << "%}" <<
              "~  %{kGraceChordMemberNote%}";
          }
        }
      }
      */

      // inside chords, a note is relative to the preceding one
      switch (gLilypondOptions->fOctaveEntryKind) {
        case kOctaveEntryRelative:
          fCurrentOctaveEntryReference = note;
          break;
        case kOctaveEntryAbsolute:
          break;
        case kOctaveEntryFixed:
          break;
      } // switch
      break;

    case msrNote::kChordMemberNote:
      {
        // print the note name
        fLilypondCodeIOstream <<
          notePitchAsLilypondString (note);

        // don't print the note duration,
        // it will be printed for the chord itself

        // don't print the string number if any,
        // it should appear after the chord itself
        const list<S_msrTechnicalWithInteger>&
          chordMemberNoteTechnicalsWithIntegers =
            note->getNoteTechnicalWithIntegers ();

        if (chordMemberNoteTechnicalsWithIntegers.size ()) {
          list<S_msrTechnicalWithInteger>::const_iterator i;
          for (
            i=chordMemberNoteTechnicalsWithIntegers.begin ();
            i!=chordMemberNoteTechnicalsWithIntegers.end ();
            i++
          ) {
            S_msrTechnicalWithInteger
              technicalWithInteger = (*i);

            switch (technicalWithInteger->getTechnicalWithIntegerKind ()) {
              case msrTechnicalWithInteger::kFingering:
                break;
              case msrTechnicalWithInteger::kFret:
                break;
              case msrTechnicalWithInteger::kString:
                if (fOnGoingChord) {
                  fPendingChordMemberNotesStringNumbers.push_back (
                    technicalWithInteger->
                        getTechnicalWithIntegerValue ());
                }
                break;
            } // switch
          } // for
        }

        // inside chords, a note is relative to the preceding one
        switch (gLilypondOptions->fOctaveEntryKind) {
          case kOctaveEntryRelative:
            fCurrentOctaveEntryReference = note;
            break;
          case kOctaveEntryAbsolute:
            break;
          case kOctaveEntryFixed:
            break;
        } // switch
      }
      break;

    case msrNote::kTupletMemberNote:
      if (gLilypondOptions->fIndentTuplets) {
        fLilypondCodeIOstream << endl;
      }

      // print the note name
      if (note->getNoteIsARest ()) {
        fLilypondCodeIOstream <<
          string (
            note->getNoteOccupiesAFullMeasure ()
              ? "s" // JMI ??? "R"
              : "r");
      }
      else {
        fLilypondCodeIOstream <<
          notePitchAsLilypondString (note);
      }

      // print the note display duration
      fLilypondCodeIOstream <<
        durationAsLilypondString (
          inputLineNumber,
          note->
            getNoteDisplayWholeNotes ());

/* JMI
      // print the tie if any
      {
        S_msrTie noteTie = note->getNoteTie ();

        if (noteTie) {
          if (noteTie->getTieKind () == msrTie::kTieStart) {
            fLilypondCodeIOstream <<
              "%{line: " << inputLineNumber << "%}" <<
              "~  %{kTupletMemberNote%}"; // JMI spaces???
          }
        }
      }
*/

      // a rest is no relative octave reference,
      if (! note->getNoteIsARest ()) {
        // this note is the new relative octave reference
        switch (gLilypondOptions->fOctaveEntryKind) {
          case kOctaveEntryRelative:
            fCurrentOctaveEntryReference = note;
            break;
          case kOctaveEntryAbsolute:
            break;
          case kOctaveEntryFixed:
            break;
        } // switch
      }
      break;

    case msrNote::kGraceTupletMemberNote:
      if (gLilypondOptions->fIndentTuplets) {
        fLilypondCodeIOstream << endl;
      }

      // print the note name
      if (note->getNoteIsARest ()) {
        fLilypondCodeIOstream <<
          string (
            note->getNoteOccupiesAFullMeasure ()
              ? "R"
              : "r");
      }
      else {
        fLilypondCodeIOstream <<
          notePitchAsLilypondString (note);
      }

      // print the note display duration
      fLilypondCodeIOstream <<
        durationAsLilypondString (
          inputLineNumber,
          note->
            getNoteDisplayWholeNotes ());

      // print the tie if any
      {
        S_msrTie noteTie = note->getNoteTie ();

        if (noteTie) {
          if (noteTie->getTieKind () == msrTie::kTieStart) {
            fLilypondCodeIOstream <<
              "%{line: " << inputLineNumber << "%}" <<
              "~  %{kGraceTupletMemberNote%}"; // JMI spaces???
          }
        }
      }

      // this note is no new relative octave reference JMI ???
      // this note is the new relative octave reference
      switch (gLilypondOptions->fOctaveEntryKind) {
        case kOctaveEntryRelative:
          fCurrentOctaveEntryReference = note;
          break;
        case kOctaveEntryAbsolute:
          break;
        case kOctaveEntryFixed:
          break;
      } // switch
      break;

    case msrNote::kTupletMemberUnpitchedNote:
      if (gLilypondOptions->fIndentTuplets) {
        fLilypondCodeIOstream << endl;
      }

      // print the note name
      fLilypondCodeIOstream <<
        "e"; // by convention

      // print the note (display) duration
      fLilypondCodeIOstream <<
        durationAsLilypondString (
          inputLineNumber,
          note->
            getNoteDisplayWholeNotes ());

/* JMI
      // print the tie if any
      {
        S_msrTie noteTie = note->getNoteTie ();

        if (noteTie) {
          if (noteTie->getTieKind () == msrTie::kTieStart) {
            fLilypondCodeIOstream <<
              "%{line: " << inputLineNumber << "%}" <<
              "~  %{kTupletMemberUnpitchedNote%}";
          }
        }
      }
      */
      break;
  } // switch

  fLilypondCodeIOstream << ' ';
}

void lpsr2LilypondTranslator::generateCodeAfterNote (
  S_msrNote note)
{
}
//________________________________________________________________________
/* JMI
string lpsr2LilypondTranslator::durationAsExplicitLilypondString (
  int      inputLineNumber,
  rational wholeNotes)
{
  string result;

  result =
    wholeNotesAsLilypondString (
      inputLineNumber,
      wholeNotes);

  return result;
}
*/

//________________________________________________________________________
void lpsr2LilypondTranslator::generateNoteArticulation (
  S_msrArticulation articulation)
{
  // should the placement be generated?
  bool doGeneratePlacement = true;

  // generate note articulation preamble if any
  switch (articulation->getArticulationKind ()) {
    case msrArticulation::kAccent:
       break;
    case msrArticulation::kBreathMark:
      doGeneratePlacement = false;
      break;
    case msrArticulation::kCaesura:
      doGeneratePlacement = false;
      break;
    case msrArticulation::kSpiccato:
      doGeneratePlacement = false;
      break;
    case msrArticulation::kStaccato:
      doGeneratePlacement = true;
      break;
    case msrArticulation::kStaccatissimo:
      doGeneratePlacement = true;
      break;
    case msrArticulation::kStress:
      doGeneratePlacement = false;
      break;
    case msrArticulation::kUnstress:
      doGeneratePlacement = false;
      break;
    case msrArticulation::kDetachedLegato:
      doGeneratePlacement = true;
      break;
    case msrArticulation::kStrongAccent:
      doGeneratePlacement = true;
      break;
    case msrArticulation::kTenuto:
      doGeneratePlacement = true;
      break;

    case msrArticulation::kFermata:
      doGeneratePlacement = true;
      break;

    case msrArticulation::kArpeggiato:
      // this is handled in chordArticulationAsLilyponString ()
      doGeneratePlacement = false;
      break;
    case msrArticulation::kNonArpeggiato:
      // this is handled in chordArticulationAsLilyponString ()
      doGeneratePlacement = false;
      break;

    case msrArticulation::kDoit:
      doGeneratePlacement = true;
      break;
    case msrArticulation::kFalloff:
      doGeneratePlacement = true;
      break;
    case msrArticulation::kPlop:
      doGeneratePlacement = false;
      break;
    case msrArticulation::kScoop:
      doGeneratePlacement = false;
      break;
  } // switch

  if (doGeneratePlacement) {
    switch (articulation->getArticulationPlacementKind ()) {
      case kPlacementNone:
        fLilypondCodeIOstream << "-";
        break;
      case kPlacementAbove:
        fLilypondCodeIOstream << "^";
        break;
      case kPlacementBelow:
        fLilypondCodeIOstream << "_";
        break;
    } // switch
  }

  // generate note articulation itself
  switch (articulation->getArticulationKind ()) {
    case msrArticulation::kAccent:
      fLilypondCodeIOstream << ">";
      break;
    case msrArticulation::kBreathMark:
      fLilypondCodeIOstream << "\\breathe";
      break;
    case msrArticulation::kCaesura:
    /* JMI
          fLilypondCodeIOstream <<
            endl <<
            R"(\once\override BreathingSign.text = \markup {\musicglyph #"scripts.caesura.straight"} \breathe)" <<
            endl;
     */
      fLilypondCodeIOstream <<
        endl <<
        "\\override BreathingSign.text = \\markup {"
        "\\musicglyph #\"scripts.caesura.curved\"}" <<
        endl <<
        "\\breathe" <<
        endl;
      break;
    case msrArticulation::kSpiccato:
      fLilypondCodeIOstream <<
        "%{spiccato???%}";
      break;
    case msrArticulation::kStaccato:
      fLilypondCodeIOstream <<
        ".";
      break;
    case msrArticulation::kStaccatissimo:
      fLilypondCodeIOstream <<
        "!";
      break;
    case msrArticulation::kStress:
      fLilypondCodeIOstream <<
        "%{stress???%}";
      break;
    case msrArticulation::kUnstress:
      fLilypondCodeIOstream <<
        "%{unstress???%}";
      break;
    case msrArticulation::kDetachedLegato:
      fLilypondCodeIOstream <<
        "_"; // portato
      break;
    case msrArticulation::kStrongAccent:
      fLilypondCodeIOstream <<
        "^"; // marcato
      break;
    case msrArticulation::kTenuto:
      fLilypondCodeIOstream <<
        "-";
      break;

    case msrArticulation::kFermata:
      if (
        // fermata?
        S_msrFermata
          fermata =
            dynamic_cast<msrFermata*>(&(*articulation))
        ) {
        switch (fermata->getFermataTypeKind ()) {
          case msrFermata::kFermataTypeNone:
            // no placement needed
            break;
          case msrFermata::kFermataTypeUpright:
            // no placement needed
            break;
          case msrFermata::kFermataTypeInverted:
            fLilypondCodeIOstream << "_";
            break;
        } // switch

        switch (fermata->getFermataKind ()) {
          case msrFermata::kNormalFermataKind:
            fLilypondCodeIOstream << "\\fermata ";
            break;
          case msrFermata::kAngledFermataKind:
            fLilypondCodeIOstream << "\\shortfermata ";
            break;
          case msrFermata::kSquareFermataKind:
            fLilypondCodeIOstream << "\\longfermata ";
            break;
        } // switch
      }
      else {
        stringstream s;

        s <<
          "note articulation '" <<
          articulation->asString () <<
          "' has 'fermata' kind, but is not of type S_msrFermata" <<
          ", line " << articulation->getInputLineNumber ();

        msrInternalError (
          gGeneralOptions->fInputSourceName,
          articulation->getInputLineNumber (),
          __FILE__, __LINE__,
          s.str ());
      }
      break;

    case msrArticulation::kArpeggiato:
      // this is handled in chordArticulationAsLilyponString ()
      break;
    case msrArticulation::kNonArpeggiato:
      // this is handled in chordArticulationAsLilyponString ()
      break;
    case msrArticulation::kDoit:
      fLilypondCodeIOstream <<
        "\\bendAfter #+4";
      break;
    case msrArticulation::kFalloff:
      fLilypondCodeIOstream <<
        "\\bendAfter #-4";
      break;
    case msrArticulation::kPlop:
      fLilypondCodeIOstream <<
        "%{plop???%}";
      break;
    case msrArticulation::kScoop:
      fLilypondCodeIOstream <<
        "%{scoop???%}";
      break;
  } // switch
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateChordArticulation (
  S_msrArticulation articulation)
{
  switch (articulation->getArticulationPlacementKind ()) {
    case kPlacementNone:
      fLilypondCodeIOstream << "-";
      break;
    case kPlacementAbove:
      fLilypondCodeIOstream << "^";
      break;
    case kPlacementBelow:
      fLilypondCodeIOstream << "_";
      break;
  } // switch

  switch (articulation->getArticulationKind ()) {
    case msrArticulation::kAccent:
      fLilypondCodeIOstream << ">";
      break;
    case msrArticulation::kBreathMark:
      fLilypondCodeIOstream << "\\breathe";
      break;
    case msrArticulation::kCaesura:
    /* JMI
          fLilypondCodeIOstream <<
            endl <<
            R"(\once\override BreathingSign.text = \markup {\musicglyph #"scripts.caesura.straight"} \breathe)" <<
            endl;
     */
      fLilypondCodeIOstream <<
        endl <<
        "\\override BreathingSign.text = \\markup {"
        "\\musicglyph #\"scripts.caesura.curved\"}" <<
        endl <<
      "\\breathe" <<
        endl;
      break;
    case msrArticulation::kSpiccato:
      fLilypondCodeIOstream <<
        "%{spiccato???%}";
      break;
    case msrArticulation::kStaccato:
      fLilypondCodeIOstream <<
        "\\staccato"; // JMI "-.";
      break;
    case msrArticulation::kStaccatissimo:
      fLilypondCodeIOstream << "!";
      break;
    case msrArticulation::kStress:
      fLilypondCodeIOstream <<
        "%{stress???%}";
      break;
    case msrArticulation::kUnstress:
      fLilypondCodeIOstream <<
        "%{unstress%}";
      break;
    case msrArticulation::kDetachedLegato:
      fLilypondCodeIOstream <<
        "_"; // portato
      break;
    case msrArticulation::kStrongAccent:
      fLilypondCodeIOstream <<
        "^"; // marcato
      break;
    case msrArticulation::kTenuto:
      fLilypondCodeIOstream << "-";
      break;

    case msrArticulation::kFermata:
      if (
        // fermata?
        S_msrFermata
          fermata =
            dynamic_cast<msrFermata*>(&(*articulation))
        ) {
        switch (fermata->getFermataTypeKind ()) {
          case msrFermata::kFermataTypeNone:
            // no placement needed
            break;
          case msrFermata::kFermataTypeUpright:
            // no placement needed
            break;
          case msrFermata::kFermataTypeInverted:
            fLilypondCodeIOstream << "_";
            break;
        } // switch

        switch (fermata->getFermataKind ()) {
          case msrFermata::kNormalFermataKind:
            fLilypondCodeIOstream << "\\fermata ";
            break;
          case msrFermata::kAngledFermataKind:
            fLilypondCodeIOstream << "\\shortfermata ";
            break;
          case msrFermata::kSquareFermataKind:
            fLilypondCodeIOstream << "\\longfermata ";
            break;
        } // switch
      }
      else {
        stringstream s;

        s <<
          "chord articulation '" <<
          articulation->asString () <<
          "' has 'fermata' kind, but is not of type S_msrFermata" <<
          ", line " << articulation->getInputLineNumber ();

        msrInternalError (
          gGeneralOptions->fInputSourceName,
          articulation->getInputLineNumber (),
          __FILE__, __LINE__,
          s.str ());
      }
      break;

    case msrArticulation::kArpeggiato:
      fLilypondCodeIOstream <<
        "\\arpeggio";
      break;
    case msrArticulation::kNonArpeggiato:
      fLilypondCodeIOstream <<
        "\\arpeggio";
      break;
    case msrArticulation::kDoit:
      fLilypondCodeIOstream <<
        "\\bendAfter #+4";
      break;
    case msrArticulation::kFalloff:
      fLilypondCodeIOstream <<
        "\\bendAfter #-4";
      break;
    case msrArticulation::kPlop:
      fLilypondCodeIOstream <<
        "%{plop%}";
      break;
    case msrArticulation::kScoop:
      fLilypondCodeIOstream <<
        "%{scoop%}";
      break;
  } // switch
}

//________________________________________________________________________
string lpsr2LilypondTranslator::technicalAsLilypondString (
  S_msrTechnical technical)
{
  string result;

  switch (technical->getTechnicalKind ()) {
    case msrTechnical::kArrow:
      result = "%{\\Arrow???%}";
      break;
    case msrTechnical::kDoubleTongue:
      result = "-\\tongue #2";
      break;
    case msrTechnical::kDownBow:
      result = "\\downbow";
      break;
    case msrTechnical::kFingernails:
      result = "%{\\Fingernails???%}";
      break;
    case msrTechnical::kHarmonic:
      result = "\\flageolet"; // JMI "\\once\\override Staff.NoteHead.style = #'harmonic-mixed";
      break;
    case msrTechnical::kHeel:
      result = "\\lheel"; // rheel??? JMI
      break;
    case msrTechnical::kHole:
      result = "%{\\Hole???%}";
      break;
    case msrTechnical::kOpenString:
      result = "\\open"; // halfopen ??? JMI
      break;
    case msrTechnical::kSnapPizzicato:
      result = "\\snappizzicato";
      break;
    case msrTechnical::kStopped:
      result = "\\stopped"; // or -+ JMI
      break;
    case msrTechnical::kTap:
      result = "%{\\Tap???%}";
      break;
    case msrTechnical::kThumbPosition:
      result = "\\thumb";
      break;
    case msrTechnical::kToe:
      result = "\\ltoe"; // rtoe ??? JMI
      break;
    case msrTechnical::kTripleTongue:
      result = "-\\tongue #3";
      break;
    case msrTechnical::kUpBow:
      result = "\\upbow";
      break;
  } // switch

  return result;
}

//________________________________________________________________________
string lpsr2LilypondTranslator::technicalWithIntegerAsLilypondString (
  S_msrTechnicalWithInteger technicalWithInteger)
{
  stringstream s;

  switch (technicalWithInteger->getTechnicalWithIntegerKind ()) {
     case msrTechnicalWithInteger::kFingering:
      s <<
        "- " <<
       technicalWithInteger->
          getTechnicalWithIntegerValue ();
      break;
    case msrTechnicalWithInteger::kFret:
      // LilyPond will take care of that JMI
      break;
    case msrTechnicalWithInteger::kString:
      s << // no space is allowed between the backSlash and the number
        "\\" <<
       technicalWithInteger->
          getTechnicalWithIntegerValue ();
      break;
  } // switch

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::technicalWithFloatAsLilypondString (
  S_msrTechnicalWithFloat technicalWithFloat)
{
  stringstream s;

  switch (technicalWithFloat->getTechnicalWithFloatKind ()) {
    case msrTechnicalWithFloat::kBend:
      s <<
        "\\bendAfter " <<
       technicalWithFloat->
          getTechnicalWithFloatValue ();
      break;
  } // switch

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::technicalWithStringAsLilypondString (
  S_msrTechnicalWithString technicalWithString)
{
  string result;

  switch (technicalWithString->getTechnicalWithStringKind ()) {
    case msrTechnicalWithString::kHammerOn:
      break;
    case msrTechnicalWithString::kHandbell:
      result = "%{\\Handbell???%}";
      break;
    case msrTechnicalWithString::kOtherTechnical:
      result = "%{\\OtherTechnical???%}";
      break;
    case msrTechnicalWithString::kPluck:
      result = "%{Pluck???%}";
      break;
    case msrTechnicalWithString::kPullOff:
      break;
  } // switch

  string stringValue =
    technicalWithString->
      getTechnicalWithStringValue ();

  if (stringValue.size ()) {
    result +=
      string (" ") +
      "-\\markup {\"" + stringValue + "\"}";
  }

  return result;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateOrnament (
  S_msrOrnament ornament)
{
  S_msrNote
    ornamentNoteUpLink =
      ornament->
        getOrnamentNoteUpLink ();

  string
    noteUpLinkDuration =
      ornamentNoteUpLink->
        noteSoundingWholeNotesAsMsrString ();

  switch (ornament->getOrnamentKind ()) {
    case msrOrnament::kOrnamentTrill:
      if (! ornamentNoteUpLink->getNoteWavyLineSpannerStart ()) {
        fLilypondCodeIOstream <<
          "\\trill ";
      }
      else {
        fLilypondCodeIOstream <<
          "\\startTrillSpan ";
      }
      break;

    case msrOrnament::kOrnamentDashes:
      if (! ornamentNoteUpLink->getNoteWavyLineSpannerStart ()) {
        fLilypondCodeIOstream <<
          "%{\\dashes%} ";
      }
      break;

    case msrOrnament::kOrnamentTurn:
      fLilypondCodeIOstream <<
          "\\turn ";
      break;

    case msrOrnament::kOrnamentInvertedTurn:
      fLilypondCodeIOstream <<
          "\\reverseturn ";
      break;

    case msrOrnament::kOrnamentDelayedTurn:
      {
        // c2*2/3  s2*1/3\turn
        rational
          remainingFraction =
            rational (1, 1)
              -
            gLilypondOptions->fDelayedOrnamentsFraction;

        int
          numerator =
            remainingFraction.getNumerator (),
          denominator =
            remainingFraction.getDenominator ();

        fLilypondCodeIOstream <<
          "s" <<
          noteUpLinkDuration <<
          "*" <<
            denominator
            -
            numerator <<
          "/" <<
            denominator <<
          "\\turn ";

        // forget about the last found whole notes duration,
        // since the latter has been multipled by fDelayedOrnamentsFraction
        fLastMetWholeNotes = rational (0, 1);
      }
      break;

    case msrOrnament::kOrnamentDelayedInvertedTurn:
      {
/* JMI
        fLilypondCodeIOstream <<
          "delayed inverted turn is not supported, replaced by inverted turn," <<
          endl <<
          "see http://lilypond.org/doc/v2.18/Documentation/snippets/expressive-marks";

        lpsrMusicXMLWarning (
          inputLineNumber,
          s.str ());

        result = "\\reverseturn %{ " + s.str () + " %}";
*/
        // c2*2/3 ( s2*1/3\turn

        fLilypondCodeIOstream <<
          "s" <<
          noteUpLinkDuration <<
          "*1/3" "\\reverseturn ";
      }
      break;

    case msrOrnament::kOrnamentVerticalTurn:
      fLilypondCodeIOstream <<
        "^\\markup { \\rotate #90 \\musicglyph #\"scripts.turn\" } ";
          /* JMI
      {
        string message =
          "delayed vertical turn is not supported, ignored";

        lpsrMusicXMLWarning (
          inputLineNumber,
          message);

        result = "%{ " + message + " %}";
      }
        */
      break;

    case msrOrnament::kOrnamentMordent:
      fLilypondCodeIOstream <<
        "\\mordent ";
      break;

    case msrOrnament::kOrnamentInvertedMordent:
      fLilypondCodeIOstream <<
        "\\prall ";
      break;
      \
    case msrOrnament::kOrnamentSchleifer:
      fLilypondCodeIOstream <<
        "%{\\schleifer???%} ";
      break;

    case msrOrnament::kOrnamentShake:
      fLilypondCodeIOstream <<
        "%{\\shake???%} ";
      break;

    case msrOrnament::kOrnamentAccidentalMark:
      switch (ornament->getOrnamentPlacementKind ()) {
        case kPlacementNone:
          fLilypondCodeIOstream << "-";
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          fLilypondCodeIOstream << "_";
          break;
      } // switch

      switch (ornament->getOrnamentAccidentalMark ()) {
        case kTripleFlat:
          fLilypondCodeIOstream << "\\markup { \\tripleflat } ";
          break;
        case kDoubleFlat:
          fLilypondCodeIOstream << "\\markup { \\doubleflat } ";
          break;
        case kSesquiFlat:
          fLilypondCodeIOstream << "\\markup { \\sesquiflat } ";
          break;
        case kFlat:
          fLilypondCodeIOstream << "\\markup { \\flat } ";
          break;
        case kSemiFlat:
          fLilypondCodeIOstream << "\\markup { \\semiflat } ";
          break;
        case kNatural:
          fLilypondCodeIOstream << "\\markup { \\natural } ";
          break;
        case kSemiSharp:
          fLilypondCodeIOstream << "\\markup { \\semisharp } ";
          break;
        case kSharp:
          fLilypondCodeIOstream << "\\markup { \\sharp } ";
          break;
        case kSesquiSharp:
          fLilypondCodeIOstream << "\\markup { \\sesquisharp } ";
          break;
        case kDoubleSharp:
          fLilypondCodeIOstream << "\\markup { \\doublesharp } ";
          break;
        case kTripleSharp:
          fLilypondCodeIOstream << "\\markup { \\triplesharp } ";
          break;
        case k_NoAlteration:
          break;
      } // switch
      break;
  } // switch
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateCodeForSpannerBeforeNote (
  S_msrSpanner spanner)
{
  switch (spanner->getSpannerKind ()) {
    case msrSpanner::kSpannerDashes:
      switch (spanner->getSpannerTypeKind ()) {
        case kSpannerTypeStart:
          fLilypondCodeIOstream <<
            "\\once \\override TextSpanner.style = #'dashed-line" <<
            endl;
          fOnGoingTrillSpanner = true;
          break;
        case kSpannerTypeStop:
          break;
        case kSpannerTypeContinue:
          break;
        case k_NoSpannerType:
          break;
      } // switch
      break;

    case msrSpanner::kSpannerWavyLine:
      switch (spanner->getSpannerTypeKind ()) {
        case kSpannerTypeStart:
          if (spanner->getSpannerNoteUpLink ()->getNoteTrillOrnament ()) {
            // don't generate anything, the trill will display the wavy line
            fOnGoingTrillSpanner = true;
          }
          else {
            fLilypondCodeIOstream <<
              "\\once \\override TextSpanner.style = #'trill" <<
              endl;
          }
          break;
        case kSpannerTypeStop:
          break;
        case kSpannerTypeContinue:
          break;
        case k_NoSpannerType:
          break;
      } // switch

      msrPlacementKind
        spannerPlacementKind =
          spanner->getSpannerPlacementKind ();

      if (spannerPlacementKind != fCurrentSpannerPlacementKind) {
        switch (spannerPlacementKind) {
          case msrPlacementKind::kPlacementNone:
            break;
          case msrPlacementKind::kPlacementAbove:
            fLilypondCodeIOstream <<
              "\\textSpannerUp ";
            break;
          case msrPlacementKind::kPlacementBelow:
            fLilypondCodeIOstream <<
              "\\textSpannerDown ";
            break;
          } // switch

        fCurrentSpannerPlacementKind = spannerPlacementKind;
      }
      break;
  } // switch
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateCodeForSpannerAfterNote (
  S_msrSpanner spanner)
{
  switch (spanner->getSpannerKind ()) {
    case msrSpanner::kSpannerDashes:
      switch (spanner->getSpannerTypeKind ()) {
        case kSpannerTypeStart:
          fLilypondCodeIOstream <<
            "\\startTextSpan ";
          fOnGoingTrillSpanner = true;
          break;
        case kSpannerTypeStop:
          fLilypondCodeIOstream <<
            "\\stopTextSpan ";
          fOnGoingTrillSpanner = false;
          break;
        case kSpannerTypeContinue:
          break;
        case k_NoSpannerType:
          break;
      } // switch
      break;

    case msrSpanner::kSpannerWavyLine:
      switch (spanner->getSpannerTypeKind ()) {
        case kSpannerTypeStart:
          if (spanner->getSpannerNoteUpLink ()->getNoteTrillOrnament ()) {
            // don't generate anything, the trill will display the wavy line
            fOnGoingTrillSpanner = true;
          }
          else {
            fLilypondCodeIOstream <<
              "\\startTextSpan " <<
              endl;
          }
          break;
        case kSpannerTypeStop:
          {
            // get spanner start end
            S_msrSpanner
              spannerStartEnd =
                spanner->
                  getSpannerOtherEndSideLink ();

            // sanity check
            msrAssert (
              spannerStartEnd != nullptr,
              "spannerStartEnd is null");

            // has the start end a trill ornament?
            if (spannerStartEnd->getSpannerNoteUpLink ()->getNoteTrillOrnament ()) {
              fLilypondCodeIOstream <<
                "\\stopTrillSpan ";
            }
            else {
              fLilypondCodeIOstream <<
                "\\stopTextSpan ";
            }
            fOnGoingTrillSpanner = false;
          }
          break;
        case kSpannerTypeContinue:
          break;
        case k_NoSpannerType:
          break;
      } // switch

/* JMI
      msrPlacementKind
        spannerPlacementKind =
          spanner->getSpannerPlacementKind ();

      if (spannerPlacementKind != fCurrentSpannerPlacementKind) {
        switch (spannerPlacementKind) {
          case msrPlacementKind::kPlacementNone:
            break;
          case msrPlacementKind::kPlacementAbove:
            fLilypondCodeIOstream <<
              "\\textSpannerUp ";
            break;
          case msrPlacementKind::kPlacementBelow:
            fLilypondCodeIOstream <<
              "\\textSpannerDown ";
            break;
          } // switch

        fCurrentSpannerPlacementKind = spannerPlacementKind;
      }
      */

      break;
  } // switch
}

//________________________________________________________________________
string lpsr2LilypondTranslator::dynamicsAsLilypondString (
  S_msrDynamics dynamics)
{
  string result =
    "\\" + dynamics->dynamicsKindAsString ();

  return result;
}

//________________________________________________________________________
string lpsr2LilypondTranslator::harpPedalTuningAsLilypondString (
  msrAlterationKind alterationKind)
{
  string result;

  switch (alterationKind) {
    case kTripleFlat:
      result = "%{ tripleFlat %} ";
      break;
    case kDoubleFlat:
      result = "%{ doubleFlat %} ";
      break;
    case kSesquiFlat:
      result = "%{ sesquiFlat %} ";
      break;
    case kFlat:
      result = "^";
      break;
    case kSemiFlat:
      result = "?";
      break;
    case kNatural:
      result = "-";
      break;
    case kSemiSharp:
      result = "?";
      break;
    case kSharp:
      result = "v";
      break;
    case kSesquiSharp:
      result = "%{ sesquiSharp %} ";
      break;
    case kDoubleSharp:
      result = "%{ doubleSharp %} ";
      break;
    case kTripleSharp:
      result = "%{ tripleSharp %} ";
      break;
    case k_NoAlteration:
      result = "alteration???";
      break;
  } // switch

  return result;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::transposeDiatonicError (
  int inputLineNumber,
  int transposeDiatonic,
  int transposeChromatic)
{
  stringstream s;

  s <<
    "diatonic '" << transposeDiatonic <<
    "' is not consistent with " <<
    "chromatic '" << transposeChromatic <<
    "'";

  msrMusicXMLError (
    gGeneralOptions->fInputSourceName,
    inputLineNumber,
    __FILE__, __LINE__,
    s.str ());
}

//________________________________________________________________________
string lpsr2LilypondTranslator::singleTremoloDurationAsLilypondString (
  S_msrSingleTremolo singleTremolo)
{
  int
    singleTremoloMarksNumber =
      singleTremolo->
        getSingleTremoloMarksNumber ();

/* JMI
  S_msrNote
    singleTremoloNoteUpLink =
      singleTremolo->
        getSingleTremoloNoteUpLink ();
*/

  msrDurationKind
    singleTremoloNoteDurationKind =
      singleTremolo->
        getSingleTremoloGraphicDurationKind ();
    /*
      singleTremoloNoteUpLink->
        getNoteGraphicDurationKind ();
    */

  /*
  The same output can be obtained by adding :N after the note,
  where N indicates the duration of the subdivision (it must be at least 8).
  If N is 8, one beam is added to the note’s stem.
  */

  int durationToUse =
    singleTremoloMarksNumber; // JMI / singleTremoloNoteSoundingWholeNotes;

  if (singleTremoloNoteDurationKind >= kEighth) {
    durationToUse +=
      1 + (singleTremoloNoteDurationKind - kEighth);
  }

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTremolos) {
    fLogOutputStream <<
      "singleTremoloDurationAsLilypondString()" <<
      ", line " << singleTremolo->getInputLineNumber () <<
      ", " <<
      singularOrPlural (
        singleTremoloMarksNumber, "mark", "marks") <<
      ", singleTremoloNoteDurationKind : " <<
      singleTremoloNoteDurationKind <<
      ", durationToUse : " <<
      durationToUse <<
      endl;
  }
#endif

  stringstream s;

  s <<
    ":" <<
 // JMI   int (pow (2, durationToUse + 2)) <<
      int (1 << (durationToUse + 2)) <<
    ' ';

  return s.str ();
}

//________________________________________________________________________
string lpsr2LilypondTranslator::harmonyDegreeAlterationKindAsLilypondString (
  msrAlterationKind harmonyDegreeAlterationKind)
{
  string result;

  switch (harmonyDegreeAlterationKind) {
    case k_NoAlteration:
      result = "?";
      break;
    case kTripleFlat:
      result = "?";
      break;
    case kDoubleFlat:
      result = "?";
      break;
    case kSesquiFlat:
      result = "?";
      break;
    case kFlat:
      result = "-";
      break;
    case kSemiFlat:
      result = "?";
      break;
    case kNatural:
      result = "";
      break;
    case kSemiSharp:
      result = "?";
      break;
    case kSharp:
      result = "+";
      break;
    case kSesquiSharp:
      result = "?";
      break;
    case kDoubleSharp:
      result = "?";
      break;
    case kTripleSharp:
      result = "?";
      break;
  } // switch

  return result;
}

string lpsr2LilypondTranslator::harmonyAsLilypondString (
  S_msrHarmony harmony)
{
  int inputLineNumber =
    harmony->getInputLineNumber ();

  stringstream s;

  // should '\powerChords' be generated?
  switch (harmony->getHarmonyKind ()) {
    case kPowerHarmony:
      if (! fPowerChordHaveAlreadyBeenGenerated) {
        s << "\\powerChords ";
        fPowerChordHaveAlreadyBeenGenerated = true;
      }
      break;
    default:
      ;
  } // switch

  // print harmony pitch
  s <<
    msrQuarterTonesPitchKindAsString (
      gMsrOptions->
        fMsrQuarterTonesPitchesLanguageKind,
      harmony->
        getHarmonyRootQuarterTonesPitchKind ());

  // print harmony duration
  msrTupletFactor
    harmonyTupletFactor =
      harmony->getHarmonyTupletFactor ();

  if (harmonyTupletFactor.isEqualToOne ()) {
    // use harmony sounding whole notes
    s <<
      wholeNotesAsLilypondString (
        inputLineNumber,
        harmony->
          getHarmonySoundingWholeNotes ());
  }
  else {
    // use harmony display whole notes and tuplet factor
    s <<
      wholeNotesAsLilypondString (
        inputLineNumber,
        harmony->
          getHarmonyDisplayWholeNotes ()) <<
      "*" <<
      rational (1, 1) / harmonyTupletFactor.asRational ();
  }

  // print harmony kind
  switch (harmony->getHarmonyKind ()) {
    case k_NoHarmony:
      s << "Harmony???";
      break;

    // MusicXML chords

    case kMajorHarmony:
      s << ":5.3";
      break;
    case kMinorHarmony:
      s << ":m";
      break;
    case kAugmentedHarmony:
      s << ":aug";
      break;
    case kDiminishedHarmony:
      s << ":dim";
      break;

    case kDominantHarmony:
      s << ":7";
      break;
    case kMajorSeventhHarmony:
      s << ":maj7";
      break;
    case kMinorSeventhHarmony:
      s << ":m7";
      break;
    case kDiminishedSeventhHarmony:
      s << ":dim7";
      break;
    case kAugmentedSeventhHarmony:
      s << ":aug7";
      break;
    case kHalfDiminishedHarmony:
      s << ":m7.5-";
      break;
    case kMinorMajorSeventhHarmony:
      s << ":m7+";
      break;

    case kMajorSixthHarmony:
      s << ":6";
      break;
    case kMinorSixthHarmony:
      s << ":m6";
      break;

    case kDominantNinthHarmony:
      s << ":9";
      break;
    case kMajorNinthHarmony:
      s << ":maj7.9";
      break;
    case kMinorNinthHarmony:
      s << ":m7.9";
      break;

    case kDominantEleventhHarmony:
      s << ":11";
      break;
    case kMajorEleventhHarmony:
      s << ":maj7.11";
      break;
    case kMinorEleventhHarmony:
      s << ":m7.11";
      break;

    case kDominantThirteenthHarmony:
      s << ":13";
      break;
    case kMajorThirteenthHarmony:
      s << ":maj7.13";
      break;
    case kMinorThirteenthHarmony:
      s << ":m7.13";
      break;

    case kSuspendedSecondHarmony:
      s << ":sus2";
      break;
    case kSuspendedFourthHarmony:
      s << ":sus4";
      break;

/*
 * kNeapolitan f aes des' in:
 *
 * c e g c' -> f f aes des' -> d g d b -> c e g c'

they are three different pre-dominant chords that are taught to American undergrads in a sophomore theory course.

in E major:
Italian = C E A#
French = C E F# A#
German = C E G A#
Tristan = C D# F# A#

in all of them, the C and A# in theory want to fan out to B (the dominant).  This is, of course, in theory - Wagner’s use of the Tristan chord, which he clearly named his opera after, has the A# moving down to A, or the 7th of the dominant (I’m transposing to fit w/ the example above).  Wagner obviously did not pay much attention during his sophomore music theory course…
*/
    case kNeapolitanHarmony:
      s << ":3-.6-%{:Neapolitan%}";
      break;
    case kItalianHarmony:
      s << ":3.6+%{:Italian%}";
      break;
    case kFrenchHarmony:
      s << ":3.5+.6+%{:French%}";
      break;
    case kGermanHarmony:
      s << ":3.5.6+%{:German%}";
      break;

    case kPedalHarmony:
      s << "%{:Pedal%}";
      break;
    case kPowerHarmony:
      s << ":5 %{power%}";
      break;
    case kTristanHarmony:
      s << ":2+.5+.6+%{:Tristan%}";
      break;

    // jazz-specific chords

    case kMinorMajorNinth: // -maj9, minmaj9
      s << ":m9";
      break;

    case kDominantSuspendedFourthHarmony: // 7sus4, domsus4
      s << ":7sus4";
      break;
    case kDominantAugmentedFifthHarmony: // 7#5, domaug5
      s << ":7.5+";
      break;
    case kDominantMinorNinthHarmony: // 7b9, dommin9
      s << ":7.9-";
      break;
    case kDominantAugmentedNinthDiminishedFifthHarmony: // 7#9b5, domaug9dim5
      s << ":7.9+.5-";
      break;
    case kDominantAugmentedNinthAugmentedFifthHarmony: // 7#9#5, domaug9aug5
      s << ":7.9+.5+";
      break;
    case kDominantAugmentedEleventhHarmony: // 7#11, domaug11
      s << ":7.11+";
      break;

    case kMajorSeventhAugmentedEleventhHarmony: // maj7#11, maj7aug11
      s << ":maj7.11+";
      break;

    // other

    case kOtherHarmony:
      s << "%{:Other%}";
      break;

    case kNoneHarmony:
      s << "%{:None%}";
      break;
  } // switch

  // print harmony degrees if any
  list<S_msrHarmonyDegree>
    harmonyDegreesList =
      harmony->getHarmonyDegreesList ();

  if (harmonyDegreesList.size ()) {
    bool thereAreDegreesToBeRemoved = false;

    // print degrees to be added if any first
    for (
      list<S_msrHarmonyDegree>::const_iterator i = harmonyDegreesList.begin ();
      i != harmonyDegreesList.end ();
      i++
    ) {
      S_msrHarmonyDegree harmonyDegree = (*i);

      // get harmony degree information
      int
        harmonyDegreeValue =
          harmonyDegree->getHarmonyDegreeValue ();

      msrAlterationKind
        harmonyDegreeAlterationKind =
          harmonyDegree->
            getHarmonyDegreeAlterationKind ();

      msrHarmonyDegree::msrHarmonyDegreeTypeKind
        harmonyDegreeTypeKind =
          harmonyDegree->
            getHarmonyDegreeTypeKind ();

      // print the harmony degree
      switch (harmonyDegreeTypeKind) {
        case msrHarmonyDegree::kHarmonyDegreeTypeAdd:
          s <<
      // JMI ???      "." <<
            harmonyDegreeValue <<
            harmonyDegreeAlterationKindAsLilypondString (
              harmonyDegreeAlterationKind);
          break;

        case msrHarmonyDegree::kHarmonyDegreeTypeAlter:
          s <<
            "." <<
            harmonyDegreeValue <<
            harmonyDegreeAlterationKindAsLilypondString (
              harmonyDegreeAlterationKind);
          break;

        case msrHarmonyDegree::kHarmonyDegreeTypeSubstract:
          thereAreDegreesToBeRemoved = true;
          break;
      } // switch
    } // for

    // then print harmony degrees to be removed if any
    if (thereAreDegreesToBeRemoved) {
      s << "^";

      int counter = 0;

      for (
        list<S_msrHarmonyDegree>::const_iterator i = harmonyDegreesList.begin ();
        i != harmonyDegreesList.end ();
        i++
      ) {
        counter++;

        S_msrHarmonyDegree
          harmonyDegree = (*i);

        // get harmony degree information
        int
          harmonyDegreeValue =
            harmonyDegree->getHarmonyDegreeValue ();

        msrAlterationKind
          harmonyDegreeAlterationKind =
            harmonyDegree->
              getHarmonyDegreeAlterationKind ();

        msrHarmonyDegree::msrHarmonyDegreeTypeKind
          harmonyDegreeTypeKind =
            harmonyDegree->
              getHarmonyDegreeTypeKind ();

        // print the harmony degree
        switch (harmonyDegreeTypeKind) {
          case msrHarmonyDegree::kHarmonyDegreeTypeAdd:
          case msrHarmonyDegree::kHarmonyDegreeTypeAlter:
            break;

          case msrHarmonyDegree::kHarmonyDegreeTypeSubstract:
   // JMI         if (counter > 1) {}
          s <<
            harmonyDegreeValue <<
            harmonyDegreeAlterationKindAsLilypondString (
              harmonyDegreeAlterationKind);
            break;
        } // switch
      } // for
    }
  }

  // print the harmony bass if relevant
  msrQuarterTonesPitchKind
    harmonyBassQuarterTonesPitchKind =
      harmony->
        getHarmonyBassQuarterTonesPitchKind ();

  if (harmonyBassQuarterTonesPitchKind != k_NoQuarterTonesPitch_QTP) {
    s <<
      "/" <<
      msrQuarterTonesPitchKindAsString (
        gMsrOptions->
          fMsrQuarterTonesPitchesLanguageKind,
        harmonyBassQuarterTonesPitchKind);
  }

  // print the harmony inversion if relevant // JMI ???
  int harmonyInversion =
    harmony->getHarmonyInversion ();

  if ( harmonyInversion!= K_HARMONY_NO_INVERSION) {
    s <<
      "%{ inversion: " << harmonyInversion << " %}";
  }

  return s.str ();
}

//________________________________________________________________________
bool compareFrameNotesByDecreasingStringNumber (
  const S_msrFrameNote& first,
  const S_msrFrameNote& second)
{
  return
    first->getFrameNoteStringNumber ()
      >
    second->getFrameNoteStringNumber ();
}

string lpsr2LilypondTranslator::frameAsLilypondString (
  S_msrFrame frame)
{
/* JMI
  int inputLineNumber =
    frame->getInputLineNumber ();
  */

  stringstream s;

  list<S_msrFrameNote>
    frameFrameNotesList =
      frame->getFrameFrameNotesList ();

  const list<msrBarre>&
    frameBarresList =
      frame->getFrameBarresList ();

  int frameStringsNumber =
    frame->getFrameStringsNumber ();
  int frameFretsNumber =
    frame->getFrameFretsNumber ();

  s <<
    "^\\markup {\\fret-diagram #\"";

  // are there fingerings?
  if (frame->getFrameContainsFingerings ()) {
    s <<
      "f:1;";
  }

  // strings number
  if (frameStringsNumber != 6) {
    s <<
      "w:" <<
      frameStringsNumber <<
      ";";
  }

  // frets number
  s <<
    "h:" <<
    frameFretsNumber <<
    ";";

  // frame barres
  if (frameBarresList.size ()) {
    list<msrBarre>::const_iterator
      iBegin = frameBarresList.begin (),
      iEnd   = frameBarresList.end (),
      i      = iBegin;

    for ( ; ; ) {
      msrBarre barre = (*i);

      s <<
        "c:" <<
        barre.fBarreStartString <<
        "-" <<
        barre.fBarreStopString <<
        "-" <<
        barre.fBarreFretNumber <<
        ";";

      if (++i == iEnd) break;
  // JMI    os << ";";
    } // for
  }

  // frame notes
  if (frameFrameNotesList.size ()) {
    // sort the frame notes,
    // necessary both for code generation
    // and the detection of muted (i.e. absent) frame notes
    frameFrameNotesList.sort (
      compareFrameNotesByDecreasingStringNumber);

    int currentStringNumber = frameStringsNumber;

    // generate the code
    list<S_msrFrameNote>::const_iterator
      iBegin = frameFrameNotesList.begin (),
      iEnd   = frameFrameNotesList.end (),
      i      = iBegin;

    for ( ; ; ) {
      S_msrFrameNote
        frameNote = (*i);

      int frameNoteStringNumber =
        frameNote->getFrameNoteStringNumber ();
      int frameNoteFretNumber =
        frameNote->getFrameNoteFretNumber ();
      int frameNoteFingering =
        frameNote->getFrameNoteFingering ();

      while (currentStringNumber > frameNoteStringNumber) {
        // currentStringNumber is missing,
        // hence it is a muted note
        s <<
          currentStringNumber <<
          "-x;";

        currentStringNumber--;
      }

      // generate code for the frame note
      s <<
        frameNoteStringNumber <<
        "-";

      if (frameNoteFretNumber == 0) {
        s <<
          "o";
      }
      else {
        s <<
          frameNoteFretNumber;
      }

      if (frameNoteFingering != -1) {
        s <<
          "-" <<
          frameNoteFingering;
      }

      s <<
        ";";

      currentStringNumber--;

      if (++i == iEnd) break;
  // JMI    os << ";";
    } // for

    // handling low-numbered muted notes
    while (currentStringNumber != 0) {
      // currentStringNumber is missing,
      // hence it is a muted note
      s <<
        currentStringNumber <<
        "-x;";

      currentStringNumber--;
    }
  }

  s <<
    "\" } ";

  return s.str ();
}

void lpsr2LilypondTranslator::generateInputLineNumberAndOrPositionInMeasureAsAComment (
  S_msrMeasureElement measureElement)
{
  fLilypondCodeIOstream <<
    "%{ ";

  if (gLilypondOptions->fInputLineNumbers) {
    // print the input line number as a comment
    fLilypondCodeIOstream <<
      "line: " << measureElement->getInputLineNumber () << " ";
  }

  if (gLilypondOptions->fPositionsInMeasures) {
    // print the position in measure as a comment
    fLilypondCodeIOstream <<
      "pim: " << measureElement->getPositionInMeasure () << " ";
  }

  fLilypondCodeIOstream <<
    "%} ";
}

//________________________________________________________________________
string lpsr2LilypondTranslator::generateMultilineName (string theString)
{
  stringstream s;

  s <<
     "\\markup { \\center-column { ";

  list<string> chunksList;

  splitRegularStringAtEndOfLines (
    theString,
    chunksList);

  if (chunksList.size ()) {
    /*
      \markup { \center-column { // JMI ???
        \line {"Long"} \line {"Staff"} \line {"Name"}
        } }
    */

    // generate a markup containing the chunks
    list<string>::const_iterator
      iBegin = chunksList.begin (),
      iEnd   = chunksList.end (),
      i      = iBegin;

    for ( ; ; ) {
      s <<
        "\\line { \"" << (*i) << "\" }";
      if (++i == iEnd) break;
      s << ' ';
    } // for

    s <<
      " } } " <<
      "% " << chunksList.size () << " chunk(s)" <<
      endl;
  }

  return s.str ();
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrScore& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrScore" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // initial empty line in LilyPond code
  // to help copy/paste it
// JMI  fLilypondCodeIOstream << endl;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrScore& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrScore" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // final empty line in LilyPond code
  // to help copy/paste it
  fLilypondCodeIOstream << endl;
}

    // names

string lpsr2LilypondTranslator::nameAsLilypondString (
  string name)
{
  string result;

  size_t endOfLineFound = name.find ("\n");

  if (endOfLineFound != string::npos) {
    result =
      generateMultilineName (name);
  }
  else {
    result = "\"" + name + "\"";
  }

  return result;
}

string lpsr2LilypondTranslator::lpsrVarValAssocKindAsLilypondString (
  lpsrVarValAssoc::lpsrVarValAssocKind
    lilyPondVarValAssocKind)
{
  string result;

  switch (lilyPondVarValAssocKind) {
    // library

    case lpsrVarValAssoc::kLibraryVersion:
      result = "version";
      break;

    // MusicXML informations

    case lpsrVarValAssoc::kMusicXMLWorkNumber:
      result = "workNumber";
      break;
    case lpsrVarValAssoc::kMusicXMLWorkTitle:
      result = "title";
      break;
    case lpsrVarValAssoc::kMusicXMLMovementNumber:
      result = "movementNumber";
      break;
    case lpsrVarValAssoc::kMusicXMLMovementTitle:
      result = "subtitle";
      break;
    case lpsrVarValAssoc::kMusicXMLEncodingDate:
      result = "encodingDate";
      break;
    case lpsrVarValAssoc::kMusicXMLScoreInstrument:
      result = "scoreInstrument";
      break;
    case lpsrVarValAssoc::kMusicXMLMiscellaneousField:
      result = "miscellaneousField";
      break;

    // LilyPond informations

    case lpsrVarValAssoc::kLilypondDedication:
      result = "dedication";
      break;

    case lpsrVarValAssoc::kLilypondPiece:
      result = "piece";
      break;
    case lpsrVarValAssoc::kLilypondOpus:
      result = "opus";
      break;

    case lpsrVarValAssoc::kLilypondTitle:
      result = "title";
      break;
    case lpsrVarValAssoc::kLilypondSubTitle:
      result = "subtitle";
      break;
    case lpsrVarValAssoc::kLilypondSubSubTitle:
      result = "subsubtitle";
      break;

    case lpsrVarValAssoc::kLilypondInstrument:
      result = "instrument";
      break;
    case lpsrVarValAssoc::kLilypondMeter:
      result = "meter";
      break;

    case lpsrVarValAssoc::kLilypondTagline:
      result = "tagline";
      break;
    case lpsrVarValAssoc::kLilypondCopyright:
      result = "copyright";
      break;

    case lpsrVarValAssoc::kLilypondMyBreak:
      result = "myBreak";
      break;
    case lpsrVarValAssoc::kLilypondMyPageBreak:
      result = "myPageBreak";
      break;
    case lpsrVarValAssoc::kLilypondGlobal:
      result = "global";
      break;
  } // switch

  return result;
}

string lpsr2LilypondTranslator::lpsrVarValAssocAsLilypondString (
  S_lpsrVarValAssoc lpsrVarValAssoc,
  int               fieldNameWidth)
{
  stringstream s;

  s << left <<
    setw (fieldNameWidth) <<
    lpsrVarValAssocKindAsLilypondString (
      lpsrVarValAssoc->
        getLilyPondVarValAssocKind ()) <<
    " = ";

  msrFontStyleKind
    varValFontStyleKind =
      lpsrVarValAssoc->
        getVarValFontStyleKind ();

  bool italicIsNeeded = false;

  switch (varValFontStyleKind) {
    case kFontStyleNone:
      break;
    case kFontStyleNormal:
      break;
    case KFontStyleItalic:
      italicIsNeeded = true;
      break;
    } // switch

  msrFontWeightKind
    varValFontWeightKind =
      lpsrVarValAssoc->
        getVarValFontWeightKind ();

  bool boldIsNeeded = false;

  switch (varValFontWeightKind) {
    case kFontWeightNone:
      break;
    case kFontWeightNormal:
      break;
    case kFontWeightBold:
      boldIsNeeded = true;
      break;
    } // switch

  bool markupIsNeeded = italicIsNeeded || boldIsNeeded;

  if (markupIsNeeded) {
    fLilypondCodeIOstream << "\\markup { ";
  }

  if (italicIsNeeded) {
    fLilypondCodeIOstream << "\\italic ";
  }
  if (boldIsNeeded) {
    fLilypondCodeIOstream << "\\bold ";
  }

  s <<
    "\"" <<
    lpsrVarValAssoc->
      getVariableValue () <<
    "\"";

  if (markupIsNeeded) {
    s << " }";
  }

  return s.str ();
}

string lpsr2LilypondTranslator::lpsrVarValsListAssocKindAsLilypondString (
  lpsrVarValsListAssoc::lpsrVarValsListAssocKind
    lilyPondVarValsListAssocKind)
{
  string result;

  switch (lilyPondVarValsListAssocKind) {
    // MusicXML informations

    case lpsrVarValsListAssoc::kMusicXMLRights:
      result = "rights";
      break;
    case lpsrVarValsListAssoc::kMusicXMLComposer:
      result = "composer";
      break;
    case lpsrVarValsListAssoc::kMusicXMLArranger:
      result = "arranger";
      break;
    case lpsrVarValsListAssoc::kMusicXMLPoet:
      result = "poet";
      break;
    case lpsrVarValsListAssoc::kMusicXMLLyricist:
      result = "lyricist";
      break;
    case lpsrVarValsListAssoc::kMusicXMLTranslator:
      result = "translator";
      break;
    case lpsrVarValsListAssoc::kMusicXMLArtist:
      result = "artist";
      break;
    case lpsrVarValsListAssoc::kMusicXMLSoftware:
      result = "software";
      break;
    } // switch

  return result;
}

void lpsr2LilypondTranslator::generateLpsrVarValsListAssocValues (
  S_lpsrVarValsListAssoc varValsListAssoc)
{
  const list<string>&
    variableValuesList =
      varValsListAssoc->
        getVariableValuesList ();

  switch (variableValuesList.size ()) {
    case 0:
      break;

    case 1:
      // generate a single string
      fLilypondCodeIOstream <<
        "\"" <<
        escapeDoubleQuotes (variableValuesList.front ()) <<
        "\"";
      break;

    default:
      // generate a markup containing the chunks
      fLilypondCodeIOstream <<
        endl <<
        "\\markup {" <<
        endl;

      gIndenter++;

      fLilypondCodeIOstream <<
        "\\column {" <<
        endl;

      gIndenter++;

      list<string>::const_iterator
        iBegin = variableValuesList.begin (),
        iEnd   = variableValuesList.end (),
        i      = iBegin;

      for ( ; ; ) {
        fLilypondCodeIOstream <<
          "\"" << (*i) << "\"";
        if (++i == iEnd) break;
        fLilypondCodeIOstream << endl;
      } // for

      fLilypondCodeIOstream << endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;
  } // switch
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrVarValAssoc& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrVarValAssoc" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // generate the comment if needed
  string
    comment =
      elt->getComment ();

  if (comment.size ()) {
    fLilypondCodeIOstream <<
      "% " << comment <<
      endl;
  }

  // generate a comment out if needed
  switch (elt->getCommentedKind ()) {
    case lpsrVarValAssoc::kCommentedYes:
      fLilypondCodeIOstream << "%";
      break;
    case lpsrVarValAssoc::kCommentedNo:
      break;
  } // switch

  // generate the backslash if needed
  switch (elt->getBackSlashKind ()) {
    case lpsrVarValAssoc::kWithBackSlashYes:
      fLilypondCodeIOstream << "\\";
      break;
    case lpsrVarValAssoc::kWithBackSlashNo:
      break;
  } // switch

  lpsrVarValAssoc::lpsrVarValAssocKind
    varValAssocKind =
      elt->getLilyPondVarValAssocKind ();

  string
    lilyPondVarValAssocKindAsLilypondString =
      lpsrVarValAssocKindAsLilypondString (
        varValAssocKind);

  // the largest variable name length in a header is 18 JMI
  int fieldWidth;

  if (fOnGoingHeader) {
    fieldWidth = 18;
  }
  else {
    fieldWidth =
      lilyPondVarValAssocKindAsLilypondString.size ();
  }

  // generate the field name
  fLilypondCodeIOstream << left<<
    setw (fieldWidth) <<
    lilyPondVarValAssocKindAsLilypondString;

  switch (elt->getVarValSeparatorKind ()) {
    case lpsrVarValAssoc::kVarValSeparatorSpace:
      fLilypondCodeIOstream << ' ';
      break;
    case lpsrVarValAssoc::kVarValSeparatorEqualSign:
      fLilypondCodeIOstream << " = ";
      break;
  } // switch

  msrFontStyleKind
    varValFontStyleKind =
      elt->getVarValFontStyleKind ();

  bool italicIsNeeded = false;

  switch (varValFontStyleKind) {
    case kFontStyleNone:
      break;
    case kFontStyleNormal:
      break;
    case KFontStyleItalic:
      italicIsNeeded = true;
      break;
    } // switch

  msrFontWeightKind
    varValFontWeightKind =
      elt->getVarValFontWeightKind ();

  bool boldIsNeeded = false;

  switch (varValFontWeightKind) {
    case kFontWeightNone:
      break;
    case kFontWeightNormal:
      break;
    case kFontWeightBold:
      boldIsNeeded = true;
      break;
    } // switch

  bool markupIsNeeded = italicIsNeeded || boldIsNeeded;

  if (markupIsNeeded) {
    fLilypondCodeIOstream << "\\markup { ";
  }

  if (italicIsNeeded) {
    fLilypondCodeIOstream << "\\italic ";
  }
  if (boldIsNeeded) {
    fLilypondCodeIOstream << "\\bold ";
  }

  // generate the quote if needed
  lpsrVarValAssoc::lpsrQuotesKind
    quotesKind =
      elt->getQuotesKind ();

  switch (quotesKind) {
    case lpsrVarValAssoc::kQuotesAroundValueYes:
      fLilypondCodeIOstream << "\"";
      break;
    case lpsrVarValAssoc::kQuotesAroundValueNo:
      break;
  } // switch

  // generate the value and unit if any
  if (elt->getUnit ().size ()) {
    fLilypondCodeIOstream <<
      setprecision (2) <<
      elt->getVariableValue ();

    fLilypondCodeIOstream <<
      "\\" <<
      elt->getUnit ();
  }
  else {
    fLilypondCodeIOstream <<
      elt->getVariableValue ();
  }

  // generate the quote if needed
  switch (quotesKind) {
    case lpsrVarValAssoc::kQuotesAroundValueYes:
      fLilypondCodeIOstream << "\"";
      break;
    case lpsrVarValAssoc::kQuotesAroundValueNo:
      break;
  } // switch

  if (markupIsNeeded) {
    fLilypondCodeIOstream << " }";
  }

  fLilypondCodeIOstream << endl;

  // generate the end line(s) if needed
  switch (elt->getEndlKind ()) {
    case lpsrVarValAssoc::kEndlNone:
      break;
    case lpsrVarValAssoc::kEndlOnce:
      fLilypondCodeIOstream << endl;
      break;
    case lpsrVarValAssoc::kEndlTwice:
      fLilypondCodeIOstream <<
        endl <<
        endl;
      break;
  } // switch
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrVarValAssoc& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrVarValAssoc" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrVarValsListAssoc& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrVarValsListAssoc" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // the largest variable name length in a header is 18 JMI
  int fieldWidth;

  string
    lilyPondVarValsListAssocKindAsString =
      elt->lilyPondVarValsListAssocKindAsString ();

  if (fOnGoingHeader) {
    fieldWidth = 18;
  }
  else {
    fieldWidth =
      lilyPondVarValsListAssocKindAsString.size ();
  }

  fLilypondCodeIOstream << left<<
    setw (fieldWidth) <<
    lpsrVarValsListAssocKindAsLilypondString (
      elt->getVarValsListAssocKind ());

  fLilypondCodeIOstream << " = ";

  generateLpsrVarValsListAssocValues (elt);

  fLilypondCodeIOstream << endl;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrVarValsListAssoc& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrVarValsListAssoc" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrSchemeVariable& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrSchemeVariable" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  string
    comment =
      elt->getComment ();

  if (comment.size ()) {
    fLilypondCodeIOstream <<
      "% " << comment <<
      endl;
  }

  switch (elt->getCommentedKind ()) {
    case lpsrSchemeVariable::kCommentedYes:
      fLilypondCodeIOstream << "% ";
      break;
    case lpsrSchemeVariable::kCommentedNo:
      break;
  } // switch

  fLilypondCodeIOstream <<
    "#(" <<
    elt->getVariableName () <<
    ' ' <<
    elt->getVariableValue () <<
    ")";

  switch (elt->getEndlKind ()) {
    case lpsrSchemeVariable::kEndlNone:
      break;
    case lpsrSchemeVariable::kEndlOnce:
      fLilypondCodeIOstream << endl;
      break;
    case lpsrSchemeVariable::kEndlTwice:
      fLilypondCodeIOstream <<
        endl <<
        endl;
      break;
  } // switch
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrSchemeVariable& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrSchemeVariable" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrHeader& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrHeader" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\header" << " {" <<
    endl;

  gIndenter++;

  // generate header elements

  int fieldNameWidth =
    elt->maxLilypondVariablesNamesLength ();

  // MusicXML informations JMI ???

  // LilyPond informations

  {
    S_lpsrVarValAssoc
      lilypondDedication =
        elt->getLilypondDedication ();

    if (lilypondDedication) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondDedication,
          fieldNameWidth) <<
        endl;
    }
  }

  {
    S_lpsrVarValAssoc
      lilypondPiece =
        elt->getLilypondPiece ();

    if (lilypondPiece) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondPiece,
          fieldNameWidth) <<
        endl;
    }
  }

  {
    S_lpsrVarValAssoc
      lilypondOpus =
        elt->getLilypondOpus ();

    if (lilypondOpus) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondOpus,
          fieldNameWidth) <<
        endl;
    }
  }

  {
    S_lpsrVarValAssoc
      lilypondTitle =
        elt->getLilypondTitle ();

    if (lilypondTitle) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondTitle,
          fieldNameWidth) <<
        endl;
    }

    S_lpsrVarValAssoc
      lilypondSubTitle =
        elt->getLilypondSubTitle ();

    if (lilypondSubTitle) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondSubTitle,
          fieldNameWidth) <<
        endl;
    }

    S_lpsrVarValAssoc
      lilypondSubSubTitle =
        elt->getLilypondSubSubTitle ();

    if (lilypondSubSubTitle) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondSubSubTitle,
          fieldNameWidth) <<
        endl;
    }
  }

  {
    S_lpsrVarValAssoc
      lilypondInstrument =
        elt->getLilypondInstrument ();

    if (lilypondInstrument) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondInstrument,
          fieldNameWidth) <<
        endl;
    }

    S_lpsrVarValAssoc
      lilypondMeter =
        elt->getLilypondMeter ();

    if (lilypondMeter) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondMeter,
          fieldNameWidth) <<
        endl;
    }
  }

  {
    S_lpsrVarValAssoc
      lilypondCopyright =
        elt->getLilypondCopyright ();

    if (lilypondCopyright) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondCopyright,
          fieldNameWidth) <<
        endl;
    }

    S_lpsrVarValAssoc
      lilypondTagline =
        elt->getLilypondTagline ();

    if (lilypondTagline) {
      fLilypondCodeIOstream <<
        lpsrVarValAssocAsLilypondString (
          lilypondTagline,
          fieldNameWidth) <<
        endl;
    }
  }

  fOnGoingHeader = true;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrHeader& elt)
{
  gIndenter--;

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrHeader" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "}" <<
    endl << endl;

  fOnGoingHeader = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrPaper& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrPaper" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\paper" << " {" <<
    endl;

  gIndenter++;

  const int fieldWidth = 20;

  // page width, height, margins and indents

  {
    float paperWidth =
      elt->getPaperWidth ();

    if (paperWidth > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "paper-width" << " = " <<
        setprecision (3) << paperWidth << "\\cm" <<
        endl;
    }
  }

  {
    float paperHeight =
      elt->getPaperHeight ();

    if (paperHeight > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "paper-height" << " = " <<
        setprecision (3) << paperHeight << "\\cm" <<
        endl;
    }
  }

  {
    float topMargin =
      elt->getTopMargin ();

    if (topMargin > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "top-margin" << " = " <<
        setprecision (3) << topMargin << "\\cm" <<
        endl;
    }
  }

  {
    float bottomMargin =
      elt->getBottomMargin ();

    if (bottomMargin > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "bottom-margin" << " = " <<
        setprecision (3) << bottomMargin << "\\cm" <<
        endl;
    }
  }

  {
    float leftMargin =
      elt->getLeftMargin ();

    if (leftMargin > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "left-margin" << " = " <<
        setprecision (3) << leftMargin << "\\cm" <<
        endl;
    }
  }

  {
    float rightMargin =
      elt->getRightMargin ();

    if (rightMargin > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "right-margin" << " = " <<
      setprecision (3) << rightMargin << "\\cm" <<
      endl;
    }
  }

  {
    float indent =
      elt->getIndent ();

    if (indent > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "indent" << " = " <<
      setprecision (3) << indent << "\\cm" <<
      endl;
    }
    else {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "%indent" << " = " <<
      setprecision (3) << 1.5 << "\\cm" <<
      endl;
    }
  }

  {
    float shortIndent =
      elt->getShortIndent ();

    if (shortIndent > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "short-indent" << " = " <<
      setprecision (3) << shortIndent << "\\cm" <<
      endl;
    }
    else {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "%short-indent" << " = " <<
      setprecision (3) << 0.0 << "\\cm" <<
      endl;
    }
  }

  // spaces

  {
    float betweenSystemSpace =
      elt->getBetweenSystemSpace ();

    if (betweenSystemSpace > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "between-system-space" << " = " <<
        setprecision (3) << betweenSystemSpace << "\\cm" <<
        endl;
    }
  }

  {
    float pageTopSpace =
      elt->getPageTopSpace ();

    if (pageTopSpace > 0) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "page-top-space" << " = " <<
        setprecision (3) << pageTopSpace << "\\cm" <<
        endl;
    }
  }

  // headers and footers

  {
    string oddHeaderMarkup =
      elt->getOddHeaderMarkup ();

    if (oddHeaderMarkup.size ()) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "oddHeaderMarkup" << " = " <<
        oddHeaderMarkup <<
        endl;
    }
  }

  {
    string evenHeaderMarkup =
      elt->getEvenHeaderMarkup ();

    if (evenHeaderMarkup.size ()) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "evenHeaderMarkup" << " = " <<
        evenHeaderMarkup <<
        endl;
    }
  }

  {
    string oddFooterMarkup =
      elt->getOddFooterMarkup ();

    if (oddFooterMarkup.size ()) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "oddFooterMarkup" << " = " <<
        oddFooterMarkup <<
        endl;
    }
  }

  {
    string evenFooterMarkup =
      elt->getEvenFooterMarkup ();

    if (evenFooterMarkup.size ()) {
      fLilypondCodeIOstream << left <<
        setw (fieldWidth) <<
        "evenFooterMarkup" << " = " <<
        evenFooterMarkup <<
        endl;
    }
  }

  fLilypondCodeIOstream << endl;

  // generate a 'page-count' comment ready for the user
  fLilypondCodeIOstream << left <<
    setw (fieldWidth) <<
    "%" "page-count" << " = " <<
    setprecision (2) << 1 <<
    endl;

  // generate a 'system-count' comment ready for the user
  fLilypondCodeIOstream << left <<
    setw (fieldWidth) <<
    "%" "system-count" << " = " <<
    setprecision (2) << 1 <<
    endl;

  // fonts
  if (gLilypondOptions->fJazzFonts) {
    fLilypondCodeIOstream <<
R"(
  #(define fonts
     (set-global-fonts
      #:music "lilyjazz"
      #:brace "lilyjazz"
      #:roman "lilyjazz-text"
      #:sans "lilyjazz-chord"
      #:factor (/ staff-height pt 20)
      ))
)";
  }
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrPaper& elt)
{
  gIndenter--;

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrPaper" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "}" <<
    endl << endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrLayout& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrLayout" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\layout {" <<
    endl;

  gIndenter++;

  // score context
  fLilypondCodeIOstream <<
    "\\context {" <<
    endl <<
    gTab << "\\Score" <<
    endl <<
    gTab << "autoBeaming = ##f % to display tuplets brackets" <<
    endl <<
    "}" <<
    endl;

  // voice context
  if (gLilypondOptions->fAmbitusEngraver) {
    fLilypondCodeIOstream <<
      "\\context {" <<
      endl <<
      gTab << "\\Voice" <<
      endl <<
      gTab << "\\consists \"Ambitus_engraver\"" <<
      endl <<
      "}" <<
      endl;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrLayout& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrLayout" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (gLilypondOptions->fRepeatBrackets) {
    fLilypondCodeIOstream <<
      "\\context " "{" <<
      endl;

    gIndenter++;

    fLilypondCodeIOstream <<
      "\\Score" <<
      endl <<
      "% defaultBarType = #\"!\"" <<
      endl <<
      "startRepeatType = #\"[|:\"" <<
      endl <<
      "endRepeatType = #\":|]\"" <<
      endl <<
      "doubleRepeatType = #\":|][|:\"" <<
      endl;

    gIndenter--;

    fLilypondCodeIOstream <<
      "}" <<
      endl;
  }

  if (false) { // JMI XXL
    fLilypondCodeIOstream <<
      "\\context {" <<
      endl;

    gIndenter++;

    fLilypondCodeIOstream <<
      "\\Staff" <<
      endl <<
      "\\consists \"Span_arpeggio_engraver\"" <<
      endl;

    gIndenter--;

    fLilypondCodeIOstream <<
      "}" <<
      endl;
  }

  gIndenter--;

  fLilypondCodeIOstream <<
    "}" <<
    endl <<
    endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrBookBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrBookBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\book {" <<
    endl;

  gIndenter++;

/* JMI
  if (elt->getScoreBlockElements ().size ()) {
    fLilypondCodeIOstream <<
      "<<" <<
      endl;

    gIndenter++;
  }
*/
  fOnGoingScoreBlock = true;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrBookBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrBookBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/* JMI
  if (elt->getScoreBlockElements ().size ()) {
    gIndenter--;

    fLilypondCodeIOstream <<
      ">>" <<
      endl <<
      * endl;
  }
*/
  gIndenter--;

  fLilypondCodeIOstream <<
    "}" <<
    endl;

  fOnGoingScoreBlock = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrScoreBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrScoreBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\score {" <<
    endl;

  gIndenter++;

/* JMI
  if (elt->getScoreBlockElements ().size ()) {
    fLilypondCodeIOstream <<
      "<<" <<
      endl;

    gIndenter++;
  }
*/
  fOnGoingScoreBlock = true;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrScoreBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrScoreBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/* JMI
  if (elt->getScoreBlockElements ().size ()) {
    gIndenter--;

    fLilypondCodeIOstream <<
      ">>" <<
      endl <<
      * endl;
  }
*/
  gIndenter--;

  fLilypondCodeIOstream <<
    "}" <<
    endl << // JMI
    endl;

  fOnGoingScoreBlock = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrBookPartBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrBookPartBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\bookpart {" <<
    endl;

  gIndenter++;

/* JMI
  if (elt->getScoreBlockElements ().size ()) {
    fLilypondCodeIOstream <<
      "<<" <<
      endl;

    gIndenter++;
  }
*/
  fOnGoingScoreBlock = true;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrBookPartBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrBookPartBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/* JMI
  if (elt->getScoreBlockElements ().size ()) {
    gIndenter--;

    fLilypondCodeIOstream <<
      ">>" <<
      endl <<
      * endl;
  }
*/
  gIndenter--;

  fLilypondCodeIOstream <<
    "}" <<
    endl << // JMI
    endl;

  fOnGoingScoreBlock = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrParallelMusicBLock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrParallelMusicBLock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fNumberOfPartGroupBlocks =
    elt->
      getParallelMusicBLockPartGroupBlocks ().size ();

  if (fNumberOfPartGroupBlocks) {
    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        "<<" <<
        "% parallel music";
    }

    else {
      fLilypondCodeIOstream <<
        "<<";
    }

    fLilypondCodeIOstream << endl;

    gIndenter++;
  }

  fCurrentParallelMusicBLock = elt;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrParallelMusicBLock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrParallelMusicBLock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream << endl;

  if (fNumberOfPartGroupBlocks) {
    gIndenter--;

    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        ">>" <<
        "% parallel music";
    }

    else {
      fLilypondCodeIOstream <<
        ">>";
    }

    fLilypondCodeIOstream <<
      endl <<
      endl;
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrPartGroupBlock& elt)
{
  // fetch part group
  S_msrPartGroup
    partGroup =
      elt->getPartGroup ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrPartGroupBlock for '" <<
      partGroup->asShortString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fPartGroupBlocksCounter++;

  fNumberOfPartGroupBlockElements =
    elt -> getPartGroupBlockElements ().size ();

// JMI  fLilypondCodeIOstream << endl << endl << partGroup << endl << endl;

  msrPartGroup::msrPartGroupImplicitKind
    partGroupImplicitKind =
      partGroup->
        getPartGroupImplicitKind ();

  msrPartGroup::msrPartGroupSymbolKind
    partGroupSymbolKind =
      partGroup->
        getPartGroupSymbolKind ();

  msrPartGroup::msrPartGroupBarlineKind
    partGroupBarlineKind =
      partGroup->
        getPartGroupBarlineKind ();

  string
    partGroupName =
      partGroup->
        getPartGroupName (),

    partGroupAbbreviation =
      partGroup->
        getPartGroupAbbreviation (),

    partGroupInstrumentName =
      partGroup->
        getPartGroupInstrumentName ();

  // LPNR, page 567 jMI ???

  switch (partGroupImplicitKind) {
    case msrPartGroup::kPartGroupImplicitYes:
      // don't generate code for an implicit top-most part group block
      break;

    case msrPartGroup::kPartGroupImplicitNo:
      if (gLilypondOptions->fComments) {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth);
      }

      switch (partGroupSymbolKind) {
        case msrPartGroup::kPartGroupSymbolNone:
          break;

        case msrPartGroup::kPartGroupSymbolBrace: // JMI
          switch (partGroupBarlineKind) {
            case msrPartGroup::kPartGroupBarlineYes:
              fLilypondCodeIOstream <<
                "\\new PianoStaff";
              break;
            case msrPartGroup::kPartGroupBarlineNo:
              fLilypondCodeIOstream <<
                "\\new GrandStaff";
              break;
          } // switch
          break;

        case msrPartGroup::kPartGroupSymbolBracket:
          switch (partGroupBarlineKind) {
            case msrPartGroup::kPartGroupBarlineYes:
              fLilypondCodeIOstream <<
                "\\new StaffGroup";
              break;
            case msrPartGroup::kPartGroupBarlineNo:
              fLilypondCodeIOstream <<
                "\\new ChoirStaff";
              break;
          } // switch
          break;

        case msrPartGroup::kPartGroupSymbolLine:
          fLilypondCodeIOstream <<
            "\\new StaffGroup";
          break;

        case msrPartGroup::kPartGroupSymbolSquare:
          fLilypondCodeIOstream <<
            "\\new StaffGroup";
          break;
      } // switch

#ifdef TRACE_OPTIONS
      if (gTraceOptions->fTracePartGroups) {
         fLilypondCodeIOstream <<
          " %{ " <<
          partGroup->getPartGroupCombinedName () <<
          ", abs number: " <<
          partGroup->getPartGroupAbsoluteNumber () <<
          " %} ";
      }
#endif

      // should a '\with' block be generated?
      bool doGenerateAWithBlock = false;

      // generate the '\with' block if there's
      // a part group name or abbreviation to be generated
      if (
        partGroupName.size ()
          ||
        partGroupAbbreviation.size ()
      ) {
        doGenerateAWithBlock = true;
      }

      // generate the '\with' block
      // if the part group is not implicit
      switch (partGroupImplicitKind) {
        case msrPartGroup::kPartGroupImplicitYes:
          break;
        case msrPartGroup::kPartGroupImplicitNo:
          if (partGroupName.size ()) {
            doGenerateAWithBlock = true;
          }
          break;
      } // switch

      // generate the '\with' block
      // if the part group symbol is a line or square
      switch (partGroupSymbolKind) {
        case msrPartGroup::kPartGroupSymbolNone:
          break;

        case msrPartGroup::kPartGroupSymbolBrace: // JMI
          break;

        case msrPartGroup::kPartGroupSymbolBracket:
          break;

        case msrPartGroup::kPartGroupSymbolLine:
          doGenerateAWithBlock = true;
          break;

        case msrPartGroup::kPartGroupSymbolSquare:
          doGenerateAWithBlock = true;
          break;
      } // switch

      if (doGenerateAWithBlock) {
        fLilypondCodeIOstream <<
          "\\with {" <<
          endl;
      }

      gIndenter++;

      if (partGroupName.size ()) {
        fLilypondCodeIOstream <<
          "instrumentName = " <<
          nameAsLilypondString (partGroupName) <<
          endl;
      }
      if (partGroupAbbreviation.size ()) {
        fLilypondCodeIOstream <<
          "shortInstrumentName = " <<
          nameAsLilypondString (partGroupAbbreviation) <<
          endl;
      }

      switch (partGroupSymbolKind) {
        case msrPartGroup::kPartGroupSymbolNone:
          break;

        case msrPartGroup::kPartGroupSymbolBrace: // JMI
          /*
           *
           * check whether individual part have instrument names JMI
           *
            if (partGroupInstrumentName.size ()) {
              fLilypondCodeIOstream << = "\\new PianoStaff";
            }
            else {
              fLilypondCodeIOstream << = "\\new GrandStaff";
            }
              */
          break;

        case msrPartGroup::kPartGroupSymbolBracket:
          break;

        case msrPartGroup::kPartGroupSymbolLine:
          fLilypondCodeIOstream <<
            "%kPartGroupSymbolLine" <<
            endl <<
            "systemStartDelimiter = #'SystemStartBar" <<
            endl;
          break;

        case msrPartGroup::kPartGroupSymbolSquare:
          fLilypondCodeIOstream <<
            "%kPartGroupSymbolSquare" <<
            endl <<
            "systemStartDelimiter = #'SystemStartSquare" <<
            endl;
          break;
      } // switch

      gIndenter--;

      // generate the '\with' block ending
      // if the part group is not implicit
      if (doGenerateAWithBlock) {
        fLilypondCodeIOstream <<
          endl <<
          "}" <<
          endl;
      }

      if (gLilypondOptions->fComments) {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth) <<
          " <<" << "% part group " <<
          partGroup->getPartGroupCombinedName ();
      }
      else {
        fLilypondCodeIOstream <<
          " <<";
      }

      fLilypondCodeIOstream << endl;
      break;
  } // switch

  if (partGroupInstrumentName.size ()) { // JMI
    fLilypondCodeIOstream <<
      "instrumentName = \"" <<
      partGroupInstrumentName <<
      "\"" <<
      endl;
  }

  if (gLilypondOptions->fConnectArpeggios) {
    fLilypondCodeIOstream <<
      "\\set PianoStaff.connectArpeggios = ##t" <<
      endl;
  }

  fLilypondCodeIOstream << endl;

  if (elt->getPartGroupBlockElements ().size () > 1) {
    gIndenter++;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrPartGroupBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrPartGroupBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // fetch part group
  S_msrPartGroup
    partGroup =
      elt->getPartGroup ();

  if (elt->getPartGroupBlockElements ().size () > 1) {
    gIndenter--;
  }

  switch (partGroup->getPartGroupImplicitKind ()) {
    case msrPartGroup::kPartGroupImplicitYes:
      // don't generate code for an implicit top-most part group block
      break;

    case msrPartGroup::kPartGroupImplicitNo:
      if (gLilypondOptions->fComments) {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth) << ">>" <<
          "% part group " <<
          partGroup->getPartGroupCombinedName ();
      }
      else {
        fLilypondCodeIOstream <<
          ">>";
      }

      fLilypondCodeIOstream << endl;

      if (fPartGroupBlocksCounter != fNumberOfPartGroupBlocks) {
        fLilypondCodeIOstream << endl;
      }
      break;
  } // switch
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrPartBlock& elt)
{
  // fetch part block's part
  S_msrPart
    part =
      elt->getPart ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrPartBlock for '" <<
      part->asShortString () <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fPartGroupBlockElementsCounter++;

  fNumberOfStaffBlocksElements =
    elt->getPartBlockElementsList ().size ();

  if (part->getPartStavesMap ().size () > 1) {
    // don't generate code for a part with only one stave

    string
      partName =
        part->getPartName (),
      partAbbreviation =
        part->getPartAbbreviation ();
        /*
      partInstrumentName =  // JMI
        part->getPartInstrumentName (),
      partInstrumentAbbreviation =  // JMI
        part->getPartInstrumentAbbreviation ();
        */

    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        "\\new PianoStaff" <<
        " % part " << part->getPartCombinedName ();
    }
    else {
      fLilypondCodeIOstream <<
        "\\new PianoStaff";
    }
    fLilypondCodeIOstream << endl;

    // generate the 'with' block beginning
    fLilypondCodeIOstream <<
      "\\with {" <<
      endl;

    gIndenter++;

    if (partName.size ()) {
      fLilypondCodeIOstream <<
        "instrumentName = \"" <<
        partName <<
        "\"" <<
        endl;
    }

    if (partAbbreviation.size ()) {
      fLilypondCodeIOstream <<
        "shortInstrumentName = " <<
       nameAsLilypondString (partAbbreviation) <<
        endl;
    }

    if (gLilypondOptions->fConnectArpeggios) {
      fLilypondCodeIOstream <<
        "connectArpeggios = ##t" <<
        endl;
    }

    gIndenter--;

    // generate the 'with' block ending
    fLilypondCodeIOstream <<
      "}" <<
      endl;

    fLilypondCodeIOstream <<
      "<<" <<
      endl;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrPartBlock& elt)
{
  // fetch current part block's part
  S_msrPart
    part =
      elt->getPart ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrPartBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (part->getPartStavesMap ().size () > 1) {
    // don't generate code for a part with only one stave

 // JMI ???   gIndenter--;

    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) << ">>" <<
        "% part " <<
        part->getPartCombinedName ();
    }
    else {
      fLilypondCodeIOstream <<
        ">>";
    }

    fLilypondCodeIOstream << endl;

    if (fPartGroupBlockElementsCounter != fNumberOfPartGroupBlockElements) {
      fLilypondCodeIOstream << endl;
    }
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrStaffBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrStaffBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fStaffBlocksCounter++;

  S_msrStaff
    staff =
      elt->getStaff ();

  // generate the staff context
  switch (staff->getStaffKind ()) {
    case msrStaff::kStaffRegular:
      if (gLilypondOptions->fJianpu) {
        fLilypondCodeIOstream << "\\new JianpuStaff";
      }
      else {
        fLilypondCodeIOstream << "\\new Staff";
      }
      break;

    case msrStaff::kStaffTablature:
      fLilypondCodeIOstream << "\\new TabStaff";
      break;

    case msrStaff::kStaffHarmony:
      fLilypondCodeIOstream << "\\new kStaffHarmony???";
      break;

    case msrStaff::kStaffFiguredBass:
      fLilypondCodeIOstream << "\\new FiguredBassStaff???";
      break;

    case msrStaff::kStaffDrum:
      fLilypondCodeIOstream << "\\new DrumStaff";
      break;

    case msrStaff::kStaffRythmic:
      fLilypondCodeIOstream << "\\new RhythmicStaff";
      break;
    } // switch

  fLilypondCodeIOstream <<
    " = \"" <<
    staff->getStaffName () <<
    "\"";

  fLilypondCodeIOstream << endl;

  // generate the 'with' block beginning
  fLilypondCodeIOstream <<
    "\\with {" <<
    endl;

  gIndenter++;

  // fetch part upLink
  S_msrPart
    staffPartUpLink =
      staff->getStaffPartUpLink ();

  // don't generate instrument names in the staves
  // if the containing part contains several of them
  if (staffPartUpLink ->getPartStavesMap ().size () == 1) {
    // get the part upLink name to be used
    string partName =
      staffPartUpLink->
        getPartNameDisplayText ();

    if (partName.size () == 0) {
      partName =
        staffPartUpLink->
          getPartName ();
    }

    // generate the instrument name
    if (partName.size ()) {
      fLilypondCodeIOstream <<
        "instrumentName = ";

      // does the name contain hexadecimal end of lines?
      std::size_t found =
    // JMI    partName.find ("&#xd");
        partName.find ("\n");

      if (found == string::npos) {
        // no, escape quotes if any and generate the result
        fLilypondCodeIOstream <<
          "\"" <<
          escapeDoubleQuotes (partName) <<
          "\"" <<
          endl;
      }

      else {
        // yes, split the name into a chunks list
        // and generate a \markup{} // JMI ???
        fLilypondCodeIOstream <<
          endl <<
          generateMultilineName (
            partName) <<
          endl;
      }
    }

    // get the part upLink abbreviation display text to be used
    string partAbbreviation =
      staffPartUpLink->
        getPartAbbreviationDisplayText ();

    if (partAbbreviation.size () == 0) {
      partAbbreviation =
        staffPartUpLink->
          getPartAbbreviation ();
    }

    if (partAbbreviation.size ()) {
      fLilypondCodeIOstream <<
        "shortInstrumentName = ";

      // does the name contain hexadecimal end of lines?
      std::size_t found =
        partAbbreviation.find ("&#xd");

      if (found == string::npos) {
        // no, merely generate the name
        fLilypondCodeIOstream <<
          nameAsLilypondString (partAbbreviation) <<
          endl;
      }

      else {
        // yes, split the name into a chunks list
        // and generate a \markup{} // JMI ???
        fLilypondCodeIOstream <<
          endl <<
          generateMultilineName (
            partAbbreviation) <<
          endl;
      }
    }
  }

  gIndenter--;

  // generate the string tunings if any
  S_msrStaffDetails
    currentStaffDetails =
      staff->getCurrentStaffStaffDetails ();

  if (currentStaffDetails) {
    const list<S_msrStaffTuning>&
      staffTuningsList =
        currentStaffDetails->getStaffTuningsList ();

    if (staffTuningsList.size ()) {
      fLilypondCodeIOstream <<
  // JMI      "restrainOpenStrings = ##t" <<
  // JMI      endl <<
        "stringTunings = \\stringTuning <";

      list<S_msrStaffTuning>::const_iterator
        iBegin = staffTuningsList.begin (),
        iEnd   = staffTuningsList.end (),
        i      = iBegin;

      gIndenter++;

      for ( ; ; ) {
        S_msrStaffTuning
          staffTuning = (*i);

        fLilypondCodeIOstream <<
          msrQuarterTonesPitchKindAsString (
            gLpsrOptions->
              fLpsrQuarterTonesPitchesLanguageKind,
            staffTuning->
              getStaffTuningQuarterTonesPitchKind ()) <<
          absoluteOctaveAsLilypondString (
            staffTuning->getStaffTuningOctave ());

        if (++i == iEnd) break;

        fLilypondCodeIOstream << ' ';
      } // for

      fLilypondCodeIOstream <<
        ">" <<
        endl;

      gIndenter--;

      // should letters be used for frets?
      switch (currentStaffDetails->getShowFretsKind ()) {
        case msrStaffDetails::kShowFretsNumbers:
          break;
        case msrStaffDetails::kShowFretsLetters:
          fLilypondCodeIOstream <<
            "tablatureFormat = #fret-letter-tablature-format" <<
            endl;
          break;
      } // switch
    }
  }

  // generate the 'with' block ending
  fLilypondCodeIOstream <<
    "}" <<
    endl;

  // generate the comment if relevant
  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        "<<" <<
        " % staff \"" << staff->getStaffName () << "\"";
  }
  else {
    fLilypondCodeIOstream <<
      "<<";
  }

  fLilypondCodeIOstream << endl;

  if (gLilypondOptions->fJianpu) {
    fLilypondCodeIOstream <<
      " \\jianpuMusic" <<
      endl;
  }

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrStaffBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrStaffBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) << ">>" <<
      "% staff " <<
      elt->getStaff ()->getStaffName ();
  }
  else {
    fLilypondCodeIOstream <<
      ">>";
  }

  fLilypondCodeIOstream << endl;

  if (fStaffBlocksCounter != fNumberOfStaffBlocksElements) {
    fLilypondCodeIOstream << endl;
  }
}

/*
//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrNewStaffgroupBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrNewStaffgroupBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

   fLilypondCodeIOstream <<
     "\\new StaffGroup" << ' ' << "{" <<
      endl;

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrNewStaffgroupBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrNewStaffgroupBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  fLilypondCodeIOstream <<
    " }" <<
    endl << endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrNewStaffBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrNewStaffBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrNewStaffBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrNewStaffBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;
}
*/

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrUseVoiceCommand& elt) // JMI ???
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrUseVoiceCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  S_msrVoice
    voice = elt->getVoice ();

  S_msrStaff
    staff = voice-> getVoiceStaffUpLink ();

  msrStaff::msrStaffKind
    staffKind = staff->getStaffKind ();

  string staffContextName;
  string voiceContextName;

  switch (staffKind) {
    case msrStaff::kStaffRegular:
      staffContextName = "\\context Staff";
      voiceContextName = "Voice";
      break;

    case msrStaff::kStaffTablature:
      staffContextName = "\\context TabStaff";
      voiceContextName = "TabVoice";
      break;

    case msrStaff::kStaffHarmony:
      staffContextName = "\\context ChordNames2"; // JMI
      voiceContextName = "???"; // JMI
      break;

    case msrStaff::kStaffFiguredBass:
      staffContextName = "\\context FiguredBass";
      voiceContextName = "???"; // JMI
      break;

    case msrStaff::kStaffDrum:
      staffContextName = "\\context DrumStaff";
      voiceContextName = "DrumVoice";
        // the "DrumVoice" alias exists, use it
      break;

    case msrStaff::kStaffRythmic:
      staffContextName = "\\context RhythmicStaff";
      voiceContextName = "Voice";
        // no "RhythmicVoice" alias exists
      break;

  } // switch

 // if (voice->getStaffRelativeVoiceNumber () > 0) { JMI
    fLilypondCodeIOstream <<
      "\\context " << voiceContextName << " = " "\"" <<
      voice->getVoiceName () << "\"" << " <<" <<
       endl;

    gIndenter++;

    if (gLilypondOptions->fNoAutoBeaming) {
      fLilypondCodeIOstream <<
        "\\set " << staffContextName << ".autoBeaming = ##f" <<
        endl;
    }

    switch (staffKind) {
      case msrStaff::kStaffRegular:
        {
          int staffRegularVoicesCounter =
            staff->getStaffRegularVoicesCounter ();

          if (staffRegularVoicesCounter > 1) {
            switch (voice->getRegularVoiceStaffSequentialNumber ()) {
              case 1:
                fLilypondCodeIOstream << "\\voiceOne ";
                break;
              case 2:
                fLilypondCodeIOstream << "\\voiceTwo ";
                break;
              case 3:
                fLilypondCodeIOstream << "\\voiceThree ";
                break;
              case 4:
                fLilypondCodeIOstream << "\\voiceFour ";
                break;
              default:
                {}
            } // switch

            fLilypondCodeIOstream <<
              "% out of " <<
              staffRegularVoicesCounter <<
              " regular voices" <<
              endl;
          }
        }
      break;

      default:
        ;
    } // switch

    fLilypondCodeIOstream <<
      "\\" << voice->getVoiceName () << endl;

    gIndenter--;

    fLilypondCodeIOstream <<
      ">>" <<
      endl;
 // }
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrUseVoiceCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrUseVoiceCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrNewLyricsBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrNewLyricsBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (! gLilypondOptions->fNoLilypondLyrics) {
    S_msrStanza stanza = elt->getStanza ();

    fLilypondCodeIOstream <<
      "\\new Lyrics" <<
      endl;

    gIndenter++;

    fLilypondCodeIOstream <<
      "\\with {" <<
      endl <<
      gTab << "associatedVoice = " <<
      "\""  << elt->getVoice ()->getVoiceName () << "\"" <<
      endl;

    if (gMsrOptions->fAddStanzasNumbers) {
      fLilypondCodeIOstream <<
        gTab << "stanza = \"" <<
        stanza->getStanzaNumber () <<
        ".\"" <<
        endl;
    }

    fLilypondCodeIOstream <<
      "}" <<
      endl <<

      "\\" << stanza->getStanzaName () <<
      endl;

    gIndenter--;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrNewLyricsBlock& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrNewLyricsBlock" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (! gLilypondOptions->fNoLilypondLyrics) {
    // JMI
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrVariableUseCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrVariableUseCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrVariableUseCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrVariableUseCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrChordNamesContext& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrChordNamesContext" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  string
    contextTypeKindAsString =
      elt->getContextTypeKindAsString (),
    contextName =
      elt->getContextName ();

  fLilypondCodeIOstream <<
    "\\context " << contextTypeKindAsString <<
    " = \"" << contextName << "\"" <<
    endl;

/* JMI
  if (false) { //option JMI
    fLilypondCodeIOstream <<
      "\\with {" <<
      endl;

    gIndenter++;

    fLilypondCodeIOstream <<
      "\\override BarLine.bar-extent = #'(-2 . 2)" <<
      endl <<
      "\\consists \"Bar_engraver\"" <<
      endl;

    gIndenter--;

    fLilypondCodeIOstream <<
      "}" <<
      endl;
  }
      */

  fLilypondCodeIOstream <<
    "\\" << contextName <<
    endl <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrChordNamesContext& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrChordNamesContext" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrFiguredBassContext& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrFiguredBassContext" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  string
    contextTypeKindAsString =
      elt->getContextTypeKindAsString (),
    contextName =
      elt->getContextName ();

  fLilypondCodeIOstream <<
    "\\context " << contextTypeKindAsString <<
    " = \"" << contextName << "\"" <<
    endl;

/* JMI
  if (false) { //option JMI
    fLilypondCodeIOstream <<
      "\\with {" <<
      endl;

    gIndenter++;

    fLilypondCodeIOstream <<
      "\\override BarLine.bar-extent = #'(-2 . 2)" <<
      endl <<
      "\\consists \"Bar_engraver\"" <<
      endl;

    gIndenter--;

    fLilypondCodeIOstream <<
      "}" <<
      endl;
  }
  */

  fLilypondCodeIOstream <<
    "\\" << contextName <<
    endl <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrFiguredBassContext& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrFiguredBassContext" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrBarCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrBarCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrBarCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrBarCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;
}

//________________________________________________________________________
/* JMI
void lpsr2LilypondTranslator::visitStart (S_lpsrMelismaCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrMelismaCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  switch (elt->getMelismaKind ()) {
    case lpsrMelismaCommand::kMelismaStart:
// JMI      fLilypondCodeIOstream << "\\melisma ";
      break;
    case lpsrMelismaCommand::kMelismaEnd:
// JMI      fLilypondCodeIOstream << "\\melismaEnd ";
      break;
  } // switch
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrMelismaCommand& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrMelismaCommand" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}
*/

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrScore& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrScore" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrScore& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrScore" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrCredit& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrCredit" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrCredit& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrCredit" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitStart (S_msrCreditWords& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrCreditWords" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrCreditWords& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrCreditWords" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrPartGroup& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrPartGroup" <<
      ", line " << elt->getInputLineNumber () <<
      elt->getPartGroupCombinedName () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrPartGroup& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrPartGroup" <<
      elt->getPartGroupCombinedName () <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrPart& elt)
{
  string
    partCombinedName =
      elt->getPartCombinedName ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrPart" <<
      partCombinedName <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceParts) {
    fLogOutputStream <<
      endl <<
      "<!--=== part \"" << partCombinedName << "\"" <<
      ", line " << elt->getInputLineNumber () << " ===-->" <<
      endl;
  }
#endif

  fCurrentPart = elt;

  fRemainingRestMeasuresNumber = 0; // JMI
  fOnGoingRestMeasures = false; // JMI
}

void lpsr2LilypondTranslator::visitEnd (S_msrPart& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrPart" <<
      elt->getPartCombinedName () <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fCurrentPart = nullptr;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrStaff& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrStaff \"" <<
      elt->getStaffName () <<
      "\"" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fOnGoingStaff = true;
}

void lpsr2LilypondTranslator::visitEnd (S_msrStaff& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrStaff \"" <<
      elt->getStaffName () <<
      "\"" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fOnGoingStaff = false;
}

void lpsr2LilypondTranslator::visitStart (S_msrStaffTuning& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> Start visiting msrStaffTuning" <<
      endl;
  }
#endif

/* JMI
  list<S_msrStaffTuning>
    staffTuningsList =
      staff->getStaffTuningsList ();

  if (staffTuningsList.size ()) {
    // \set TabStaff.stringTunings = \stringTuning <c' g' d'' a''>

    fLilypondCodeIOstream <<
      "\\set TabStaff.stringTunings = \\stringTuning <";

    list<S_msrStaffTuning>::const_iterator
      iBegin = staffTuningsList.begin (),
      iEnd   = staffTuningsList.end (),
      i      = iBegin;

    for ( ; ; ) {
      fLilypondCodeIOstream <<
        msrQuarterTonesPitchAsString (
          gLpsrOptions->fLpsrQuarterTonesPitchesLanguage,
 // JMI            elt->getInputLineNumber (),
          ((*i)->getStaffTuningQuarterTonesPitch ())) <<
 // JMI       char (tolower ((*i)->getStaffTuningStep ())) <<
        absoluteOctaveAsLilypondString (
          (*i)->getStaffTuningOctave ());
      if (++i == iEnd) break;
      fLilypondCodeIOstream << ' ';
    } // for

    fLilypondCodeIOstream <<
      ">" <<
      endl;
  }
 */
}

void lpsr2LilypondTranslator::visitStart (S_msrStaffDetails& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> Start visiting msrStaffDetails" <<
      endl;
  }
#endif

  // fetch staff lines number
  int
    staffLinesNumber =
      elt->getStaffLinesNumber ();

  fLilypondCodeIOstream <<
    endl <<
    "\\stopStaff " <<
    endl <<
    "\\override Staff.StaffSymbol.line-count = " <<
    staffLinesNumber <<
    endl <<
    "\\startStaff" <<
    endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrVoice& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrVoice \"" <<
      elt->getVoiceName () <<
      "\"" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fCurrentVoice = elt;

  fLilypondCodeIOstream <<
    fCurrentVoice->getVoiceName () <<
    " = ";

  // generate the beginning of the voice definition
  switch (fCurrentVoice->getVoiceKind ()) {
    case msrVoice::kVoiceRegular:
      switch (gLilypondOptions->fOctaveEntryKind) {
        case kOctaveEntryRelative:
          fLilypondCodeIOstream <<
            "\\relative";
          if (
            gLilypondOptions->fRelativeOctaveEntrySemiTonesPitchAndOctave
//      JMI         !=
//            gLilypondOptions->fSemiTonesPitchAndOctaveDefaultValue
          ) {
            // option '-rel, -relative' has been used
            fLilypondCodeIOstream <<
              " " <<
              msrSemiTonesPitchAndOctaveAsLilypondString (
                gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
                gLilypondOptions->fRelativeOctaveEntrySemiTonesPitchAndOctave);
          }
          break;

        case kOctaveEntryAbsolute:
          fLilypondCodeIOstream <<
            "\\absolute";
          break;

        case kOctaveEntryFixed:
          fLilypondCodeIOstream <<
            "\\fixed " <<
            msrSemiTonesPitchAndOctaveAsLilypondString (
              gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
              gLilypondOptions->fFixedOctaveEntrySemiTonesPitchAndOctave);
          break;
      } // switch

      fLilypondCodeIOstream <<
        " {" <<
        endl;
      break;

    case msrVoice::kVoiceHarmony:
      fLilypondCodeIOstream <<
        "\\chordmode {" <<
        endl;
      break;

    case msrVoice::kVoiceFiguredBass:
      fLilypondCodeIOstream <<
        "\\figuremode {" <<
        endl;
      break;
  } // switch

  gIndenter++;

  if (gLilypondOptions->fGlobal) {
    fLilypondCodeIOstream <<
      "\\global" <<
      endl <<
      endl;
  }

  if (gLilypondOptions->fDisplayMusic) {
    fLilypondCodeIOstream <<
      "\\displayMusic {" <<
      endl;

    gIndenter++;
  }

  fLilypondCodeIOstream <<
    "\\language \"" <<
    msrQuarterTonesPitchesLanguageKindAsString (
      gLpsrOptions->
        fLpsrQuarterTonesPitchesLanguageKind) <<
    "\"" <<
    endl;

  if (gLpsrOptions->fLpsrChordsLanguageKind != k_IgnatzekChords) {
    fLilypondCodeIOstream <<
      "\\" <<
      lpsrChordsLanguageKindAsString (
        gLpsrOptions->
          fLpsrChordsLanguageKind) <<
      "Chords" <<
      endl;
  }

  if (gLilypondOptions->fShowAllBarNumbers) {
    fLilypondCodeIOstream <<
      "\\set Score.barNumberVisibility = #all-bar-numbers-visible" <<
      endl <<
      "\\override Score.BarNumber.break-visibility = ##(#f #t #t)" <<
      endl <<
      endl;
  }

// JMI   \set Score.alternativeNumberingStyle = #'numbers-with-letters

/* JMI
  if (
    fCurrentVoice->getVoiceContainsRestMeasures ()
      ||
    gLilypondOptions->fCompressRestMeasures
  ) {
    fLilypondCodeIOstream <<
      "\\compressMMRests" <<
      endl;

    gIndenter++;
  }
*/

  if (gLilypondOptions->fAccidentalStyleKind != kDefault) {
    fLilypondCodeIOstream <<
      "\\accidentalStyle Score." <<
      lpsrAccidentalStyleKindAsString (
        gLilypondOptions->fAccidentalStyleKind) <<
      endl <<
      endl;
  }

  // reset fCurrentOctaveEntryReference if relevant
  switch (gLilypondOptions->fOctaveEntryKind) {
    case kOctaveEntryRelative:
      // forget about the current reference:
      // it should be set from the LilyPond preferences here
      setCurrentOctaveEntryReferenceFromTheLilypondOptions ();
      break;
    case kOctaveEntryAbsolute:
      break;
    case kOctaveEntryFixed:
      break;
  } // switch

  fVoiceIsCurrentlySenzaMisura = false;

  fOnGoingVoice = true;

  switch (fCurrentVoice->getVoiceKind ()) { // JMI
    case msrVoice::kVoiceRegular:
      break;

    case msrVoice::kVoiceHarmony:
      fOnGoingHarmonyVoice = true;
      break;

    case msrVoice::kVoiceFiguredBass:
      fOnGoingFiguredBassVoice = true;
      break;
  } // switch

  // reset current voice measures counter
  fCurrentVoiceMeasuresCounter = 0; // none have been met

  // force durations to be displayed explicitly
  // at the beginning of the voice
  fLastMetWholeNotes = rational (0, 1);
}

void lpsr2LilypondTranslator::visitEnd (S_msrVoice& elt)
{
  gIndenter--;

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrVoice \"" <<
      elt->getVoiceName () <<
      "\"" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  /* JMI
  if (
    fCurrentVoice->getVoiceContainsRestMeasures ()
      ||
    gLilypondOptions->fCompressRestMeasures
  ) {
    fLilypondCodeIOstream <<
  // JMI    "}" <<
      endl;

    gIndenter--;
  */

  if (gLilypondOptions->fDisplayMusic) {
    fLilypondCodeIOstream <<
      "}" <<
      endl;

    gIndenter--;
  }

  // generate the end of the voice definition
  switch (elt->getVoiceKind ()) {
    case msrVoice::kVoiceRegular:
      fLilypondCodeIOstream <<
        "}" <<
        endl <<
        endl;
      break;

    case msrVoice::kVoiceHarmony:
      fLilypondCodeIOstream <<
        "}" <<
        endl <<
        endl;
      break;

    case msrVoice::kVoiceFiguredBass:
      fLilypondCodeIOstream <<
        "}" <<
        endl <<
        endl;
      break;
  } // switch

  switch (elt->getVoiceKind ()) {
    case msrVoice::kVoiceRegular:
      break;

    case msrVoice::kVoiceHarmony:
      fOnGoingHarmonyVoice = false;
      break;

    case msrVoice::kVoiceFiguredBass:
      fOnGoingFiguredBassVoice = false;
      break;
  } // switch

  fCurrentVoice = nullptr;
  fOnGoingVoice = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrVoiceStaffChange& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrVoiceStaffChange '" <<
      elt->asString () << "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    endl <<
    "\\change Staff=\"" <<
    elt->getStaffToChangeTo ()->getStaffName () <<
    "\"";

  if (
    gLilypondOptions->fInputLineNumbers
      ||
    gLilypondOptions->fPositionsInMeasures
  ) {
    generateInputLineNumberAndOrPositionInMeasureAsAComment (
      elt);
  }

  fLilypondCodeIOstream << endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrHarmony& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrHarmony '" <<
      elt->asString () <<
      "'" <<
      ", fOnGoingNote = " << booleanAsString (fOnGoingNote) <<
      ", fOnGoingChord = " << booleanAsString (fOnGoingChord) <<
      ", fOnGoingHarmonyVoice = " << booleanAsString (fOnGoingHarmonyVoice) <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (fOnGoingNote) {
  // JMI ???
  /*
#ifdef TRACE_OPTIONS
    if (gTraceOptions->fTraceHarmonies) {
      fLilypondCodeIOstream <<
        "%{ " << elt->asString () << " %}" <<
        endl;
    }
#endif
*/
  }

  else if (fOnGoingChord) { // JMI
  }

  else if (fOnGoingHarmonyVoice) {
    fLilypondCodeIOstream <<
      harmonyAsLilypondString (elt) <<
      ' ';

    if (
      gLilypondOptions->fInputLineNumbers
        ||
      gLilypondOptions->fPositionsInMeasures
    ) {
      generateInputLineNumberAndOrPositionInMeasureAsAComment (
        elt);
    }
  }

/* JMI
  else if (fOnGoingChord) {
    // don't generate code for the code harmony,
    // this will be done after the chord itself JMI
  }
  */
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrFrame& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrHarmony '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (fOnGoingNote) {
#ifdef TRACE_OPTIONS
    if (gTraceOptions->fTraceFrames) {
      fLilypondCodeIOstream <<
        "%{ " << elt->asString () << " %}" <<
        endl;
    }
#endif

    fLilypondCodeIOstream <<
      frameAsLilypondString (elt) <<
      endl;
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrFiguredBass& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrFiguredBass '" <<
      elt->asString () <<
      "'" <<
      ", fOnGoingFiguredBassVoice = " << booleanAsString (fOnGoingFiguredBassVoice) <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fCurrentFiguredBass = elt;

  if (fOnGoingNote) {
#ifdef TRACE_OPTIONS
    if (gTraceOptions->fTraceFiguredBasses) {
      fLilypondCodeIOstream <<
        "%{ FOO S_msrFiguredBass JMI " << fCurrentFiguredBass->asString () << " %}" <<
        endl;
    }
#endif
  }

  else if (fOnGoingChord) { // JMI
  }

  else if (fOnGoingFiguredBassVoice) {
    fLilypondCodeIOstream <<
      "<";

    if (
      gLilypondOptions->fInputLineNumbers
        ||
      gLilypondOptions->fPositionsInMeasures
    ) {
      generateInputLineNumberAndOrPositionInMeasureAsAComment (
        fCurrentFiguredBass);
    }
  }

  fCurrentFiguredBassFiguresCounter = 0;
}

void lpsr2LilypondTranslator::visitStart (S_msrFigure& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrFigure '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (fOnGoingFiguredBassVoice) {
    fCurrentFiguredBassFiguresCounter++;

    // is the figured bass parenthesized?
    msrFiguredBass::msrFiguredBassParenthesesKind
      figuredBassParenthesesKind =
        fCurrentFiguredBass->
          getFiguredBassParenthesesKind ();

    // generate the figure number
    switch (figuredBassParenthesesKind) {
      case msrFiguredBass::kFiguredBassParenthesesYes:
        fLilypondCodeIOstream << "[";
        break;
      case msrFiguredBass::kFiguredBassParenthesesNo:
        break;
    } // switch

    fLilypondCodeIOstream <<
      elt->getFigureNumber ();

    switch (figuredBassParenthesesKind) {
      case msrFiguredBass::kFiguredBassParenthesesYes:
        fLilypondCodeIOstream << "]";
        break;
      case msrFiguredBass::kFiguredBassParenthesesNo:
        break;
    } // switch

    // handle the figure prefix
    switch (elt->getFigurePrefixKind ()) {
      case msrFigure::k_NoFigurePrefix:
        break;
      case msrFigure::kDoubleFlatPrefix:
        fLilypondCodeIOstream << "--";
        break;
      case msrFigure::kFlatPrefix:
        fLilypondCodeIOstream << "-";
        break;
      case msrFigure::kFlatFlatPrefix:
        fLilypondCodeIOstream << "flat flat";
        break;
      case msrFigure::kNaturalPrefix:
        fLilypondCodeIOstream << "!";
        break;
      case msrFigure::kSharpSharpPrefix:
        fLilypondCodeIOstream << "sharp sharp";
        break;
      case msrFigure::kSharpPrefix:
        fLilypondCodeIOstream << "+";
        break;
      case msrFigure::kDoubleSharpPrefix:
        fLilypondCodeIOstream << "++";
        break;
    } // switch

    // handle the figure suffix
    switch (elt->getFigureSuffixKind ()) {
      case msrFigure::k_NoFigureSuffix:
        break;
      case msrFigure::kDoubleFlatSuffix:
        fLilypondCodeIOstream << "double flat";
        break;
      case msrFigure::kFlatSuffix:
        fLilypondCodeIOstream << "flat";
        break;
      case msrFigure::kFlatFlatSuffix:
        fLilypondCodeIOstream << "flat flat";
        break;
      case msrFigure::kNaturalSuffix:
        fLilypondCodeIOstream << "natural";
        break;
      case msrFigure::kSharpSharpSuffix:
        fLilypondCodeIOstream << "sharp sharp";
        break;
      case msrFigure::kSharpSuffix:
        fLilypondCodeIOstream << "sharp";
        break;
      case msrFigure::kDoubleSharpSuffix:
        fLilypondCodeIOstream << "souble sharp";
        break;
      case msrFigure::kSlashSuffix:
        fLilypondCodeIOstream << "/";
        break;
    } // switch

    // generate a space if not last figure in figured bass
    if (
      fCurrentFiguredBassFiguresCounter
        <
      fCurrentFiguredBass->getFiguredBassFiguresList ().size ()
    ) {
      fLilypondCodeIOstream << ' ';
    }
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrFiguredBass& elt)
{
  int inputLineNumber =
    elt->getInputLineNumber ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrFiguredBass '" <<
      elt->asString () <<
      "'" <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  if (fOnGoingFiguredBassVoice) {
    fLilypondCodeIOstream <<
      ">";

    // print figured bass duration
    msrTupletFactor
      figuredBassTupletFactor =
        elt->getFiguredBassTupletFactor ();

    if (figuredBassTupletFactor.isEqualToOne ()) {
      // use figured bass sounding whole notes
      fLilypondCodeIOstream <<
        wholeNotesAsLilypondString (
          inputLineNumber,
          elt->
            getFiguredBassSoundingWholeNotes ());
    }
    else {
      // use figured bass display whole notes and tuplet factor
      fLilypondCodeIOstream <<
        wholeNotesAsLilypondString (
          inputLineNumber,
          elt->
            getFiguredBassDisplayWholeNotes ()) <<
        "*" <<
        figuredBassTupletFactor.asRational ();
    }

    fLilypondCodeIOstream <<
      ' ';
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrSegment& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> Start visiting msrSegment '" <<
      elt->getSegmentAbsoluteNumber () << "'" <<
      endl;
  }
#endif

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% start of segment " <<
      elt->getSegmentAbsoluteNumber () <<
      ", line " <<
      elt->getInputLineNumber () <<
      endl;

    gIndenter++;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrSegment& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> End visiting msrSegment '" <<
      elt->getSegmentAbsoluteNumber () << "'" <<
      endl;
  }
#endif

  if (gLilypondOptions->fComments) {
    gIndenter--;

    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% end of segment" <<
      endl;
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrMeasure& elt)
{
  int
    inputLineNumber =
      elt->getInputLineNumber ();

  string
    measureNumber =
      elt->getMeasureNumber ();

  msrMeasure::msrMeasureKind
    measureKind =
      elt->getMeasureKind ();

  msrMeasure::msrMeasureEndRegularKind
    measureEndRegularKind =
      elt-> getMeasureEndRegularKind ();

  int
    measurePuristNumber =
      elt->getMeasurePuristNumber ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrMeasure '" <<
      measureNumber <<
      "', " <<
      msrMeasure::measureKindAsString (measureKind) <<
      ", " <<
      msrMeasure::measureEndRegularKindAsString (
        measureEndRegularKind) <<
      ", measurePuristNumber = '" <<
      measurePuristNumber <<
      "', onGoingRestMeasures = '" <<
      booleanAsString (
        fOnGoingRestMeasures) <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceParts) {
    fLogOutputStream <<
      endl <<
      "% <!--=== measure '" << measureNumber <<
      "' start, " <<
      msrMeasure::measureEndRegularKindAsString (
        measureEndRegularKind) <<
      ", measurePuristNumber = '" <<
      measurePuristNumber <<
      "', onGoingRestMeasures = '" <<
      booleanAsString (
        fOnGoingRestMeasures) <<
      "' , line " << inputLineNumber << " ===-->" <<
      endl;
  }
#endif

  // generate comment if relevant
  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% start of " <<
      msrMeasure::measureKindAsString (elt->getMeasureKind ()) <<
      " measure " <<
      measureNumber <<
      ", line " << inputLineNumber <<
      endl;

    gIndenter++;
  }

  // take this measure into account for counting
  switch (elt->getMeasureKind ()) {
    case msrMeasure::kMeasureKindUnknown:
      fCurrentVoiceMeasuresCounter++;
      break;
    case msrMeasure::kMeasureKindRegular:
      fCurrentVoiceMeasuresCounter++;
      break;
    case msrMeasure::kMeasureKindAnacrusis:
      // keep fCurrentVoiceMeasuresCounter at 0
      break;
    case msrMeasure::kMeasureKindIncompleteStandalone:
    case msrMeasure::kMeasureKindIncompleteLastInRepeatCommonPart:
    case msrMeasure::kMeasureKindIncompleteLastInRepeatHookedEnding:
    case msrMeasure::kMeasureKindIncompleteLastInRepeatHooklessEnding:
    case msrMeasure::kMeasureKindIncompleteNextMeasureAfterCommonPart:
    case msrMeasure::kMeasureKindIncompleteNextMeasureAfterHookedEnding:
    case msrMeasure::kMeasureKindIncompleteNextMeasureAfterHooklessEnding:
      fCurrentVoiceMeasuresCounter++;
      break;
    case msrMeasure::kMeasureKindOvercomplete:
      fCurrentVoiceMeasuresCounter++;
      break;
    case msrMeasure::kMeasureKindCadenza:
      fCurrentVoiceMeasuresCounter++;
      break;
    case msrMeasure::kMeasureKindMusicallyEmpty:
      fCurrentVoiceMeasuresCounter++;
      break;
  } // switch

  // force durations to be displayed explicitly
  // for the notes at the beginning of the measure
  fLastMetWholeNotes = rational (0, 1);

  // is this the end of a cadenza?
  if (
    fOnGoingVoiceCadenza
      &&
    measureKind != msrMeasure::kMeasureKindOvercomplete
  ) {
  /* JMI
    fLilypondCodeIOstream <<
      endl <<
      "\\cadenzaOff" <<
 // JMI     " \\undo \\omit Staff.TimeSignature" <<
      endl <<
      "\\bar \"|\" "; // JMI ???
      */

    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream <<
        " % kMeasureKindOvercomplete End";
    }

    fLilypondCodeIOstream<<
      endl;

    fOnGoingVoiceCadenza = false;
  }

  switch (measureKind) {
    case msrMeasure::kMeasureKindUnknown:
      {
        stringstream s;

        s <<
          "measure '" << measureNumber <<
          "' is of unknown kind";

if (false) // JMI
        msrInternalError (
          gGeneralOptions->fInputSourceName,
          inputLineNumber,
          __FILE__, __LINE__,
          s.str ());
else
        msrInternalWarning (
          gGeneralOptions->fInputSourceName,
          inputLineNumber,
          s.str ());
      }
      break;

    case msrMeasure::kMeasureKindRegular:
      break;

    case msrMeasure::kMeasureKindAnacrusis:
      if (elt->getMeasureFirstInVoice ()) {
        // only generate '\partial' at the beginning of a voice

        string
          upbeatDuration =
            wholeNotesAsLilypondString (
              inputLineNumber,
              elt->getCurrentMeasureWholeNotes ());

        fLilypondCodeIOstream <<
          "\\partial " << upbeatDuration <<
          endl;
      }
      break;

    case msrMeasure::kMeasureKindIncompleteStandalone:
    case msrMeasure::kMeasureKindIncompleteLastInRepeatCommonPart:
    case msrMeasure::kMeasureKindIncompleteLastInRepeatHookedEnding:
    case msrMeasure::kMeasureKindIncompleteLastInRepeatHooklessEnding:
    case msrMeasure::kMeasureKindIncompleteNextMeasureAfterCommonPart:
    case msrMeasure::kMeasureKindIncompleteNextMeasureAfterHookedEnding:
    case msrMeasure::kMeasureKindIncompleteNextMeasureAfterHooklessEnding:
      {
        rational
          currentMeasureWholeNotes =
            elt->getCurrentMeasureWholeNotes ();

        rational
          fullMeasureWholeNotes =
            elt->getFullMeasureWholeNotes ();

        // we should set the score current measure whole notes in this case
        rational
          ratioToFullMeasureWholeNotes =
            currentMeasureWholeNotes / fullMeasureWholeNotes;
        ratioToFullMeasureWholeNotes.rationalise ();

#ifdef TRACE_OPTIONS
        if (gTraceOptions->fTraceMeasuresDetails) {
          const int fieldWidth = 27;

          fLilypondCodeIOstream << left <<
            "% Setting the current measure whole notes for measure " <<
            setw (fieldWidth) <<
            measureNumber <<
            ", line = " << inputLineNumber <<
            endl <<
            setw (fieldWidth) <<
            "% currentMeasureWholeNotes" << " = " <<
            currentMeasureWholeNotes <<
            endl <<
            setw (fieldWidth) <<
            "% fullMeasureWholeNotes" << " = " <<
            fullMeasureWholeNotes <<
            endl <<
            setw (fieldWidth) <<
            "% ratioToFullMeasureWholeNotes" << " = " <<
            ratioToFullMeasureWholeNotes <<
            endl <<
            endl;
        }
#endif

        if (ratioToFullMeasureWholeNotes == rational (1, 1)) {
          stringstream s;

          s <<
            "underfull measure '" << measureNumber <<
            "' has actually the full measure whole notes";

     // JMI       msrInternalError (
          msrInternalWarning (
            gGeneralOptions->fInputSourceName,
            inputLineNumber,
    // JMI        __FILE__, __LINE__,
            s.str ());
        }

        else {
          /* JMI
          fLilypondCodeIOstream <<
            "\\set Score.measureLength = #(ly:make-moment " <<
            currentMeasureWholeNotes.toString () <<
            ")" <<
            endl;
    */

          // should we generate a break?
          if (gLilypondOptions->fBreakLinesAtIncompleteRightMeasures) {
            fLilypondCodeIOstream <<
              "\\break" <<
              endl;
          }
        }
      }
      break;

    case msrMeasure::kMeasureKindOvercomplete:
      if (! fOnGoingVoiceCadenza) {
      /* JMI
        fLilypondCodeIOstream <<
          endl <<
          "\\cadenzaOn" <<
          " \\omit Staff.TimeSignature";
*/

        if (gLilypondOptions->fComments) {
          fLilypondCodeIOstream << " % kMeasureKindOvercomplete Start";
        }

        fLilypondCodeIOstream << endl;

        fOnGoingVoiceCadenza = true;
      }
      break;

    case msrMeasure::kMeasureKindCadenza:
      if (! fOnGoingVoiceCadenza) {
        fLilypondCodeIOstream <<
          endl <<
          "\\cadenzaOn";

        if (gLilypondOptions->fComments) {
          fLilypondCodeIOstream << " % kMeasureKindCadenza Start";
        }

        fLilypondCodeIOstream << endl;

        fLilypondCodeIOstream <<
          "\\once\\omit Staff.TimeSignature" <<
          endl;

        fOnGoingVoiceCadenza = true;
      }
      break;

    case msrMeasure::kMeasureKindMusicallyEmpty:
      {
        // generate a skip the duration of the measure // JMI ???
        // followed by a bar check
        fLilypondCodeIOstream <<
  // JMI ???        "s" <<
          "R" <<
          wholeNotesAsLilypondString (
            inputLineNumber,
            elt->
              getFullMeasureWholeNotes ()) <<
          " | " <<
          endl;
      }
      break;
  } // switch
}

void lpsr2LilypondTranslator::visitEnd (S_msrMeasure& elt)
{
  int
    inputLineNumber =
      elt->getInputLineNumber ();

  string
    measureNumber =
      elt->getMeasureNumber ();

  msrMeasure::msrMeasureKind
    measureKind =
      elt->getMeasureKind ();

  msrMeasure::msrMeasureEndRegularKind
    measureEndRegularKind =
      elt-> getMeasureEndRegularKind ();

  int
    measurePuristNumber =
      elt->getMeasurePuristNumber ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrMeasure '" <<
      measureNumber <<
      "', " <<
      msrMeasure::measureKindAsString (measureKind) <<
      ", " <<
      msrMeasure::measureEndRegularKindAsString (
        measureEndRegularKind) <<
      ", measurePuristNumber = '" <<
      measurePuristNumber <<
      "', onGoingRestMeasures = '" <<
      booleanAsString (
        fOnGoingRestMeasures) <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceMeasures) {
    fLogOutputStream <<
      endl <<
      "% <!--=== measure '" << measureNumber <<
      "' end, " <<
      msrMeasure::measureEndRegularKindAsString (
        measureEndRegularKind) <<
     "' end, measurePuristNumber = '" << measurePuristNumber << "'" <<
      ", onGoingRestMeasures = '" <<
      booleanAsString (
        fOnGoingRestMeasures) <<
      "', line " << inputLineNumber << " ===-->" <<
      endl;
  }
#endif

  if (! fOnGoingRestMeasures) {
    // handle the measure
    switch (measureKind) {
      case msrMeasure::kMeasureKindUnknown: // should not occur
        fLilypondCodeIOstream <<
          "%{ measureKindUnknown, " <<
          measurePuristNumber + 1 <<
          " %}" <<
          endl;
        break;

      case msrMeasure::kMeasureKindRegular:
        {
        }
        break;

      case msrMeasure::kMeasureKindAnacrusis:
        break;

      case msrMeasure::kMeasureKindIncompleteStandalone:
        break;

      case msrMeasure::kMeasureKindIncompleteLastInRepeatCommonPart:
      case msrMeasure::kMeasureKindIncompleteLastInRepeatHookedEnding:
      case msrMeasure::kMeasureKindIncompleteLastInRepeatHooklessEnding:
      case msrMeasure::kMeasureKindIncompleteNextMeasureAfterCommonPart:
      case msrMeasure::kMeasureKindIncompleteNextMeasureAfterHookedEnding:
      case msrMeasure::kMeasureKindIncompleteNextMeasureAfterHooklessEnding:
        break;

      case msrMeasure::kMeasureKindOvercomplete:
      /* JMI
        fLilypondCodeIOstream <<
          endl <<
          "\\cadenzaOff" <<
          " \\undo \\omit Staff.TimeSignature |" <<
          endl;
          */

        fOnGoingVoiceCadenza = false;
        break;

      case msrMeasure::kMeasureKindCadenza:
        fLilypondCodeIOstream <<
          endl <<
          "\\cadenzaOff" <<
          endl <<
          "\\bar \"|\"" << // JMI ???
          endl;

        fOnGoingVoiceCadenza = false;
        break;

      case msrMeasure::kMeasureKindMusicallyEmpty: // should not occur
        fLilypondCodeIOstream <<
          "%{ emptyMeasureKind %} | % " <<
          measurePuristNumber + 1 <<
          endl;
        break;
    } // switch

    if (gLilypondOptions->fComments) {
      gIndenter--;

      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        "% end of " <<
        msrMeasure::measureKindAsString (elt->getMeasureKind ()) <<
        " measure " <<
        measureNumber <<
        ", line " << inputLineNumber <<
        endl <<
        endl;
    }

    if (gLilypondOptions->fSeparatorLineEveryNMeasures > 0) {
      if (
        fCurrentVoiceMeasuresCounter
          %
        gLilypondOptions->fSeparatorLineEveryNMeasures
          ==
        0)
        fLilypondCodeIOstream <<
          endl <<
          "% ============================= " <<
          endl <<
          endl;
    }
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrStanza& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrStanza \"" <<
      elt->getStanzaName () <<
      "\"" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (! gLilypondOptions->fNoLilypondLyrics) {
    // don't generate code for the stanza inside the code for the voice
    fGenerateCodeForOngoingNonEmptyStanza =
      ! fOnGoingVoice
        &&
      elt->getStanzaTextPresent ();

    if (fGenerateCodeForOngoingNonEmptyStanza) {
      fLilypondCodeIOstream <<
        elt->getStanzaName () << " = " << "\\lyricmode {" <<
        endl;

  //    gIndenter++; JMI ???
    }
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrStanza& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrStanza \"" <<
      elt->getStanzaName () <<
      "\"" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (! gLilypondOptions->fNoLilypondLyrics) {
    if (fGenerateCodeForOngoingNonEmptyStanza) {
 //     gIndenter--; JMI ???

      fLilypondCodeIOstream <<
        endl <<
        "}" <<
        endl <<
        endl;
    }

    fGenerateCodeForOngoingNonEmptyStanza = false;
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrSyllable& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrSyllable '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (! gLilypondOptions->fNoLilypondLyrics) {
    if (fGenerateCodeForOngoingNonEmptyStanza) {
      switch (elt->getSyllableKind ()) {
        case msrSyllable::kSyllableSingle:
          writeTextsListAsLilypondString (
            elt->getSyllableTextsList (),
            fLilypondCodeIOstream);

          fLilypondCodeIOstream <<
            elt->syllableWholeNotesAsMsrString () <<
            ' ';
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableSingle %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableBegin:
          writeTextsListAsLilypondString (
            elt->getSyllableTextsList (),
            fLilypondCodeIOstream);

          fLilypondCodeIOstream <<
            elt->syllableWholeNotesAsMsrString () <<
            " -- ";
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableBegin %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableMiddle:
          writeTextsListAsLilypondString (
            elt->getSyllableTextsList (),
            fLilypondCodeIOstream);

          fLilypondCodeIOstream <<
            elt->syllableWholeNotesAsMsrString () <<
            " -- ";
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableMiddle %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableEnd:
          writeTextsListAsLilypondString (
            elt->getSyllableTextsList (),
            fLilypondCodeIOstream);

          fLilypondCodeIOstream <<
            elt->syllableWholeNotesAsMsrString () <<
            ' ';
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableEnd %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableSkip:
          // LilyPond ignores the skip duration
          // when \lyricsto is used
          fLilypondCodeIOstream <<
            "\\skip" <<
            elt->syllableWholeNotesAsMsrString () <<
            ' ';
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableSkip %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableMeasureEnd:
      // JMI      "| " <<
          if (gLilypondOptions->fInputLineNumbers) {
            // print the measure end line number as a comment
            fLilypondCodeIOstream <<
              "%{ measure end, line " <<
              elt->getInputLineNumber () <<
              " %}";
          }

          fLilypondCodeIOstream << endl;
          break;

        case msrSyllable::kSyllableLineBreak:
          if (gLilypondOptions->fInputLineNumbers) {
            // print the measure end line number as a comment
            fLilypondCodeIOstream <<
              "%{ line break, line " <<
              elt->getInputLineNumber () <<
              " %}";
          }

          fLilypondCodeIOstream << endl;
          break;

        case msrSyllable::kSyllablePageBreak:
          if (gLilypondOptions->fInputLineNumbers) {
            // print the measure end line number as a comment
            fLilypondCodeIOstream <<
              "%{ page break, line " <<
              elt->getInputLineNumber () <<
              " %}";
          }

          fLilypondCodeIOstream << endl;
          break;

        case msrSyllable::kSyllableNone: // JMI
          break;
      } // switch

      switch (elt->getSyllableExtendKind ()) {
        case msrSyllable::kSyllableExtendSingle:
          // generate a lyric extender, i.e. a melisma,
          // after this syllable
          fLilypondCodeIOstream <<
            "__ ";
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableExtendSingle %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableExtendStart:
          // generate a lyric extender, i.e. a melisma,
          // after this syllable
          fLilypondCodeIOstream <<
            "__ ";
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableExtendStart %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableExtendContinue:
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableExtendContinue %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableExtendStop:
#ifdef TRACE_OPTIONS
          if (gTraceOptions->fTraceLyrics) {
            fLilypondCodeIOstream <<
              "%{ kSyllableExtendStop %} ";
          }
#endif
          break;

        case msrSyllable::kSyllableExtendNone:
          break;
      } // switch

    if (
      gLilypondOptions->fInputLineNumbers
        ||
      gLilypondOptions->fPositionsInMeasures
    ) {
      generateInputLineNumberAndOrPositionInMeasureAsAComment (
        elt);
      }
    }
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrSyllable& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrSyllable '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrClef& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrClef '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  msrClef::msrClefKind
    clefKind =
      elt->getClefKind ();

  if (clefKind != msrClef::k_NoClef) {
    fLilypondCodeIOstream <<
      "\\clef \"";

    switch (clefKind) {
      case msrClef::k_NoClef:
        break;
      case msrClef::kTrebleClef:
        fLilypondCodeIOstream << "treble";
        break;
      case msrClef::kSopranoClef:
        fLilypondCodeIOstream << "soprano";
        break;
      case msrClef::kMezzoSopranoClef:
        fLilypondCodeIOstream << "mezzosoprano";
        break;
      case msrClef::kAltoClef:
        fLilypondCodeIOstream << "alto";
        break;
      case msrClef::kTenorClef:
        fLilypondCodeIOstream << "tenor";
        break;
      case msrClef::kBaritoneClef:
        fLilypondCodeIOstream << "baritone";
        break;
      case msrClef::kBassClef:
        fLilypondCodeIOstream << "bass";
        break;
      case msrClef::kTrebleLine1Clef:
        fLilypondCodeIOstream << "french";
        break;
      case msrClef::kTrebleMinus15Clef:
        fLilypondCodeIOstream << "treble_15";
        break;
      case msrClef::kTrebleMinus8Clef:
        fLilypondCodeIOstream << "treble_8";
        break;
      case msrClef::kTreblePlus8Clef:
        fLilypondCodeIOstream << "treble^8";
        break;
      case msrClef::kTreblePlus15Clef:
        fLilypondCodeIOstream << "treble^15";
        break;
      case msrClef::kBassMinus15Clef:
        fLilypondCodeIOstream << "bass_15";
        break;
      case msrClef::kBassMinus8Clef:
        fLilypondCodeIOstream << "bass_8";
        break;
      case msrClef::kBassPlus8Clef:
        fLilypondCodeIOstream << "bass^8";
        break;
      case msrClef::kBassPlus15Clef:
        fLilypondCodeIOstream << "bass^15";
        break;
      case msrClef::kVarbaritoneClef:
        fLilypondCodeIOstream << "varbaritone";
        break;
      case msrClef::kTablature4Clef:
      case msrClef::kTablature5Clef:
      case msrClef::kTablature6Clef:
      case msrClef::kTablature7Clef:
        fLilypondCodeIOstream << "tab";
        /* JMI ???
        if (gLilypondOptions->fModernTab) {
          fLilypondCodeIOstream <<
            "\"moderntab\"" <<
            endl <<
            "\\tabFullNotation";
        }
        else {
          fLilypondCodeIOstream <<
            "\"tab\"" <<
            endl <<
            "\\tabFullNotation";
        }
            */
        break;
      case msrClef::kPercussionClef:
        fLilypondCodeIOstream << "percussion";
        break;
      case msrClef::kJianpuClef:
        fLilypondCodeIOstream << "%{jianpuClef???%}";
        break;
    } // switch

  fLilypondCodeIOstream <<
    "\"" <<
    endl;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrClef& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrClef '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrKey& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrKey '" <<
      elt->asString () <<
      "'" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  switch (fCurrentVoice->getVoiceKind ()) {
    case msrVoice::kVoiceRegular:
    case msrVoice::kVoiceHarmony:
      switch (elt->getKeyKind ()) {
        case msrKey::kTraditionalKind:
          fLilypondCodeIOstream <<
            "\\key " <<
            msrQuarterTonesPitchKindAsString (
              gLpsrOptions->fLpsrQuarterTonesPitchesLanguageKind,
              elt->getKeyTonicQuarterTonesPitchKind ()) <<
            " \\" <<
            msrKey::keyModeKindAsString (
              elt->getKeyModeKind ()) <<
            endl;
          break;

        case msrKey::kHumdrumScotKind:
          {
            const vector<S_msrHumdrumScotKeyItem>&
              humdrumScotKeyItemsVector =
                elt->getHumdrumScotKeyItemsVector ();

            if (humdrumScotKeyItemsVector.size ()) {
              fLilypondCodeIOstream <<
                endl <<
                "\\set Staff.keyAlterations = #`(";

              vector<S_msrHumdrumScotKeyItem>::const_iterator
                iBegin = humdrumScotKeyItemsVector.begin (),
                iEnd   = humdrumScotKeyItemsVector.end (),
                i      = iBegin;

              for ( ; ; ) {
                S_msrHumdrumScotKeyItem item = (*i);

                if (elt->getKeyItemsOctavesAreSpecified ()) {
                  //   JMI   "((octave . step) . alter) ((octave . step) . alter) ...)";
                  //\set Staff.keyAlterations = #`(((3 . 3) . 7) ((3 . 5) . 3) ((3 . 6) . 3))"  \time 2/4


                    fLilypondCodeIOstream <<
                      "(" <<
                        "(" <<
                        item->getKeyItemOctave () - 3 <<
                          // in MusicXML, octave number is 4 for the octave
                          // starting with middle C,
                          // and the latter is c' in LilyPond
                        " . " <<
                        item->getKeyItemDiatonicPitchKind () <<
                        ")" <<
                      " . ," <<
                      alterationKindAsLilypondString (
                        item->getKeyItemAlterationKind ()) <<
                      ")";
                }

                else {
                  // Alternatively, for each item in the list, using the more concise format (step . alter) specifies that the same alteration should hold in all octaves.

                    fLilypondCodeIOstream <<
                      "(" <<
                      item->getKeyItemDiatonicPitchKind () <<
                      " . ," <<
                      alterationKindAsLilypondString (
                        item->getKeyItemAlterationKind ()) <<
                      ")";
                }

                if (++i == iEnd) break;

                fLilypondCodeIOstream << ' ';
              } // for

              fLilypondCodeIOstream <<
                ")";
            }

            else {
                msrInternalError (
                  gGeneralOptions->fInputSourceName,
                  elt->getInputLineNumber (),
                  __FILE__, __LINE__,
                  "Humdrum/Scot key items vector is empty");
            }
          }
          break;
      } // switch
      break;

    case msrVoice::kVoiceFiguredBass:
      // not \key should be generated in \figuremode
      break;
  } // switch
}

void lpsr2LilypondTranslator::visitEnd (S_msrKey& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrKey '" <<
      elt->asString () <<
      "'"  <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTime& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTime " <<
      elt->asString () <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  msrTime::msrTimeSymbolKind
    timeSymbolKind =
      elt->getTimeSymbolKind ();

  const vector<S_msrTimeItem>&
    timeItemsVector =
      elt->getTimeItemsVector ();

/* JMI
  // is this the end of a senza misura fragment?
  if (
    fVoiceIsCurrentlySenzaMisura
      &&
    timeSymbolKind != msrTime::kTimeSymbolSenzaMisura) {
    fLilypondCodeIOstream <<
      "\\undo\\omit Staff.TimeSignature" <<
      endl;

    fVoiceIsCurrentlySenzaMisura = false;
  }
*/

  // handle the time
  if (timeSymbolKind == msrTime::kTimeSymbolSenzaMisura) {
    // senza misura time

    /* JMI
    fLilypondCodeIOstream <<
      "\\omit Staff.TimeSignature" <<
      endl;
*/

    fVoiceIsCurrentlySenzaMisura = true;
  }

  else {
    // con misura time

    int timesItemsNumber =
      timeItemsVector.size ();

    if (timesItemsNumber) {
      // should there be a single number?
      switch (timeSymbolKind) {
        case msrTime::kTimeSymbolCommon:
          break;
        case msrTime::kTimeSymbolCut:
          break;
        case msrTime::kTimeSymbolNote:
          break;
        case msrTime::kTimeSymbolDottedNote:
          break;
        case msrTime::kTimeSymbolSingleNumber:
          fLilypondCodeIOstream <<
            "\\once\\override Staff.TimeSignature.style = #'single-digit" <<
            endl;
          break;
        case msrTime::kTimeSymbolSenzaMisura:
          break;
        case msrTime::kTimeSymbolNone:
          break;
      } // switch

      if (! elt->getTimeIsCompound ()) {
        // simple time
        // \time "3/4" for 3/4
        // or senza misura

        S_msrTimeItem
          timeItem =
            timeItemsVector [0]; // the only element;

        // fetch the time item beat numbers vector
        const vector<int>&
          beatsNumbersVector =
            timeItem->
              getTimeBeatsNumbersVector ();

        // should the time be numeric?
        if (
          timeSymbolKind == msrTime::kTimeSymbolNone
            ||
          gLilypondOptions->fNumericalTime) {
          fLilypondCodeIOstream <<
            "\\numericTimeSignature ";
        }

        fLilypondCodeIOstream <<
          "\\time " <<
          beatsNumbersVector [0] << // the only element
          "/" <<
          timeItem->getTimeBeatValue () <<
          endl;
      }

      else {
        // compound time
        // \compoundMeter #'(3 2 8) for 3+2/8
        // \compoundMeter #'((3 8) (2 8) (3 4)) for 3/8+2/8+3/4
        // \compoundMeter #'((3 2 8) (3 4)) for 3+2/8+3/4

        fLilypondCodeIOstream <<
          "\\compoundMeter #`(";

        // handle all the time items in the vector
        for (int i = 0; i < timesItemsNumber; i++) {
          S_msrTimeItem
            timeItem =
              timeItemsVector [i];

          // fetch the time item beat numbers vector
          const vector<int>&
            beatsNumbersVector =
              timeItem->
                getTimeBeatsNumbersVector ();

          int beatsNumbersNumber =
            beatsNumbersVector.size ();

          // first generate the opening parenthesis
          fLilypondCodeIOstream <<
            "(";

          // then generate all beats numbers in the vector
          for (int j = 0; j < beatsNumbersNumber; j++) {
            fLilypondCodeIOstream <<
              beatsNumbersVector [j] <<
              ' ';
          } // for

          // then generate the beat type
          fLilypondCodeIOstream <<
            timeItem->getTimeBeatValue ();

          // and finally generate the closing parenthesis
          fLilypondCodeIOstream <<
            ")";

          if (i != timesItemsNumber - 1) {
            fLilypondCodeIOstream <<
              ' ';
          }
        } // for

      fLilypondCodeIOstream <<
        ")" <<
        endl;
      }
    }

    else {
      // there are no time items
      if (timeSymbolKind != msrTime::kTimeSymbolSenzaMisura) {
        msrInternalError (
          gGeneralOptions->fInputSourceName,
          elt->getInputLineNumber (),
          __FILE__, __LINE__,
          "time items vector is empty");
      }
    }
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrTime& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTime " <<
      elt->asString () <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTranspose& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTranspose" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  int  transposeDiatonic  = elt->getTransposeDiatonic ();
  int  transposeChromatic = elt->getTransposeChromatic ();
  int  transposeOctaveChange = elt->getTransposeOctaveChange ();
  bool transposeDouble = elt->getTransposeDouble ();

/*
  // transposition in LilyPond is relative to c',
  // i.e. the C in the middle of the piano keyboard

The diatonic element specifies the number of pitch steps needed to go from written to sounding pitch. This allows for correct spelling of enharmonic transpositions.

The chromatic element represents the number of semitones needed to get from written to sounding pitch. This value does not include octave-change values; the values for both elements need to be added to the written pitch to get the correct sounding pitch.

The octave-change element indicates how many octaves to add to get from written pitch to sounding pitch.

If the double element is present, it indicates that the music is doubled one octave down from what is currently written (as is the case for mixed cello / bass parts in orchestral literature).
*/

  // determine transposition pitch
  msrQuarterTonesPitchKind
    transpositionPitchKind = k_NoQuarterTonesPitch_QTP;

  switch (transposeChromatic) {
    case -11:
      switch (transposeDiatonic) {
        case -7:
          transpositionPitchKind = kC_Sharp_QTP;
          break;

        case -6:
          transpositionPitchKind = kD_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -10:
      switch (transposeDiatonic) {
        case -6:
          transpositionPitchKind = kD_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -9:
      switch (transposeDiatonic) {
        case -6:
          transpositionPitchKind = kD_Sharp_QTP;
          break;

        case -5:
          transpositionPitchKind = kE_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -8:
      switch (transposeDiatonic) {
        case -5:
          transpositionPitchKind = kE_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -7:
      switch (transposeDiatonic) {
        case -4:
          transpositionPitchKind = kF_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -6:
      switch (transposeDiatonic) {
        case -4:
          transpositionPitchKind = kF_Sharp_QTP;
          break;

        case -3:
          transpositionPitchKind = kG_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -5:
      switch (transposeDiatonic) {
        case -3:
          transpositionPitchKind = kG_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -4:
      switch (transposeDiatonic) {
        case -3:
          transpositionPitchKind = kG_Sharp_QTP;
          break;

        case -2:
          transpositionPitchKind = kA_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -3:
      switch (transposeDiatonic) {
        case -2:
          transpositionPitchKind = kA_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -2:
      switch (transposeDiatonic) {
        case -2:
          transpositionPitchKind = kA_Sharp_QTP;
          break;

        case -1:
          transpositionPitchKind = kB_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case -1:
      switch (transposeDiatonic) {
        case -1:
          transpositionPitchKind = kB_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 0:
      switch (transposeDiatonic) {
        case 0:
          transpositionPitchKind = kC_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 1:
      switch (transposeDiatonic) {
        case 0:
          transpositionPitchKind = kC_Sharp_QTP;
          break;

        case 1:
          transpositionPitchKind = kD_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 2:
      switch (transposeDiatonic) {
        case 1:
          transpositionPitchKind = kD_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 3:
      switch (transposeDiatonic) {
        case 1:
          transpositionPitchKind = kD_Sharp_QTP;
          break;

        case 2:
          transpositionPitchKind = kE_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 4:
      switch (transposeDiatonic) {
        case 2:
          transpositionPitchKind = kE_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 5:
      switch (transposeDiatonic) {
        case 3:
          transpositionPitchKind = kF_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 6:
      switch (transposeDiatonic) {
        case 3:
          transpositionPitchKind = kF_Sharp_QTP;
          break;

        case 4:
          transpositionPitchKind = kG_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 7:
      switch (transposeDiatonic) {
        case 4:
          transpositionPitchKind = kG_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 8:
      switch (transposeDiatonic) {
        case 4:
          transpositionPitchKind = kG_Sharp_QTP;
          break;

        case 5:
          transpositionPitchKind = kA_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 9:
      switch (transposeDiatonic) {
        case 5:
          transpositionPitchKind = kA_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 10:
      switch (transposeDiatonic) {
        case 5:
          transpositionPitchKind = kA_Sharp_QTP;
          break;

        case 6:
          transpositionPitchKind = kB_Flat_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    case 11:
      switch (transposeDiatonic) {
        case 6:
          transpositionPitchKind = kB_Natural_QTP;
          break;

        default:
          transposeDiatonicError (
            inputLineNumber,
            transposeDiatonic,
            transposeChromatic);
      } // switch
      break;

    default:
      {
        stringstream s;

        s <<
          "transpose chromatic '" << transposeChromatic <<
          "' is not between -12 and 12, ignored";

        msrMusicXMLError (
          gGeneralOptions->fInputSourceName,
          elt->getInputLineNumber (),
          __FILE__, __LINE__,
          s.str ());
      }
  } // switch

  // 4 is the octave starting with middle C
  int transpositionOctave;

  if (transposeChromatic < 0) {
    transpositionOctave = 3;
  }
  else {
    transpositionOctave = 4;
  }

  // take the transpose octave change into account
  transpositionOctave += transposeOctaveChange;

  // take the transpose double if any into account
  if (transposeDouble) {
    transpositionOctave--;
  }

  string
    transpositionPitchKindAsString =
      msrQuarterTonesPitchKindAsString (
        gLpsrOptions->
          fLpsrQuarterTonesPitchesLanguageKind,
        transpositionPitchKind);

  string
    transpositionOctaveAsString =
      absoluteOctaveAsLilypondString (
        transpositionOctave);

/* JMI
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTranspositions) {
    fLilypondCodeIOstream << // JMI
      "Handlling transpose '" <<
      elt->transposeAsString () <<
      "' ignored because it is already present in voice \"" <<
      fCurrentVoice->getVoiceName () <<
      "\"" <<
      / * JMI
      getStaffName () <<
      "\" in part " <<
      fStaffPartUpLink->getPartCombinedName () <<
      * /
      endl <<
      ", transpositionPitch: " <<
      transpositionPitchAsString <<
      ", transpositionOctave: " <<
      transpositionOctaveAsString <<
      "(" << transpositionOctave << ")" <<
      endl;
    }
#endif
*/

  // now we can generate the transpostion command
  fLilypondCodeIOstream <<
    "\\transposition " <<
    transpositionPitchKindAsString <<
    transpositionOctaveAsString <<
    ' ' <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_msrTranspose& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTranspose" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrPartNameDisplay& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrPartNameDisplay" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  string partNameDisplayValue =
    elt->getPartNameDisplayValue ();

  fLilypondCodeIOstream <<
    "\\set Staff.instrumentName = \"" <<
    partNameDisplayValue <<
    "\"" <<
    endl <<
 // JMI ???   "\\tweak break-visibility #end-of-line-visible" <<
 //   endl <<
    "\\tweak RehearsalMark.self-alignment-X #LEFT" <<
    endl <<
    "\\mark\\markup{\"" <<
    partNameDisplayValue <<
    "\"}" <<
    endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrPartAbbreviationDisplay& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrPartAbbreviationDisplay" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  string partAbbreviationDisplayValue =
    elt->getPartAbbreviationDisplayValue ();

  fLilypondCodeIOstream <<
    "\\set Staff.shortInstrumentName = " <<
    nameAsLilypondString (partAbbreviationDisplayValue) <<
    endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTempo& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTempo" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  const list<S_msrWords>&
    tempoWordsList =
      elt->getTempoWordsList ();

  int tempoWordsListSize = tempoWordsList.size ();

  msrDottedDuration tempoBeatUnit  = elt->getTempoBeatUnit ();
  string            tempoPerMinute = elt->getTempoPerMinute ();

  msrTempo::msrTempoParenthesizedKind
    tempoParenthesizedKind =
      elt->getTempoParenthesizedKind ();

  switch (elt->getTempoPlacementKind ()) {
    case msrPlacementKind::kPlacementNone:
      break;
    case msrPlacementKind::kPlacementAbove:
      break;
    case msrPlacementKind::kPlacementBelow:
      fLilypondCodeIOstream <<
        "\\once\\override Score.MetronomeMark.direction = #DOWN";
      break;
    } // switch

  switch (elt->getTempoKind ()) {
    case msrTempo::k_NoTempoKind:
      break;

    case msrTempo::kTempoBeatUnitsPerMinute:
      switch (tempoParenthesizedKind) {
        case msrTempo::kTempoParenthesizedYes:
          fLilypondCodeIOstream <<
            "\\tempo " <<
            "\\markup {" <<
            endl;

          gIndenter++;

          fLilypondCodeIOstream <<
            "\\concat {" <<
            endl;

          gIndenter++;

          gIndenter++;

          if (tempoWordsListSize) {
            list<S_msrWords>::const_iterator
              iBegin = tempoWordsList.begin (),
              iEnd   = tempoWordsList.end (),
              i      = iBegin;

            for ( ; ; ) {
              S_msrWords words = (*i);

              fLilypondCodeIOstream <<
                "\"" << words->getWordsContents () << "\"";

              if (++i == iEnd) break;

              fLilypondCodeIOstream <<
                ' ';
            } // for
          }

          fLilypondCodeIOstream <<
            "(";
          if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
            fLilypondCodeIOstream <<
              " \\smaller \\general-align #Y #DOWN \\note {";
          }
          else {
            fLilypondCodeIOstream <<
              "\\smaller \\general-align #Y #DOWN \\note #\"";
          }

          fLilypondCodeIOstream <<
            dottedDurationAsLilypondStringWithoutBackSlash (
              inputLineNumber,
              tempoBeatUnit);

          if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
            fLilypondCodeIOstream <<
              "} #UP";
          }
          else {
            fLilypondCodeIOstream <<
              "\" #UP";
          }

          fLilypondCodeIOstream <<
            endl <<
            "\" = \"" <<
            endl <<
            tempoPerMinute <<
            ")" <<
            endl;

          gIndenter--;

          fLilypondCodeIOstream << endl;

          gIndenter--;

          fLilypondCodeIOstream <<
            "}" <<
            endl;

          gIndenter--;

          fLilypondCodeIOstream <<
            "}" <<
            endl;
        break;

      case msrTempo::kTempoParenthesizedNo:
        fLilypondCodeIOstream <<
          "\\tempo " <<
          "\\markup {" <<
          endl;

        gIndenter++;

        if (tempoWordsListSize) {
          list<S_msrWords>::const_iterator
            iBegin = tempoWordsList.begin (),
            iEnd   = tempoWordsList.end (),
            i      = iBegin;

          for ( ; ; ) {
            S_msrWords words = (*i);

            fLilypondCodeIOstream <<
              "\"" << words->getWordsContents () << "\"";

            if (++i == iEnd) break;

            fLilypondCodeIOstream <<
              ' ';
          } // for
        }

        fLilypondCodeIOstream <<
          "\\concat {" <<
          endl;

        gIndenter++;

        if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
          fLilypondCodeIOstream <<
            " \\smaller \\general-align #Y #DOWN \\note {";
        }
        else {
          fLilypondCodeIOstream <<
            " \\smaller \\general-align #Y #DOWN \\note #\"";
        }

        fLilypondCodeIOstream <<
          dottedDurationAsLilypondStringWithoutBackSlash (
            inputLineNumber,
            tempoBeatUnit);

        if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
          fLilypondCodeIOstream <<
            "} #UP";
        }
        else {
          fLilypondCodeIOstream <<
            "\" #UP";
        }

        fLilypondCodeIOstream <<
          endl <<
          "\" = \"" <<
          endl <<
          tempoPerMinute <<
        endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;

      /*
        // JMI way to remove automatic parentheses if text is associated?
        fLilypondCodeIOstream <<
          "\\tempo ";

        if (tempoWordsListSize) {
          list<S_msrWords>::const_iterator
            iBegin = tempoWordsList.begin (),
            iEnd   = tempoWordsList.end (),
            i      = iBegin;

          for ( ; ; ) {
            S_msrWords words = (*i);

            fLilypondCodeIOstream <<
              "\"" << words->getWordsContents () << "\"";

            if (++i == iEnd) break;

            fLilypondCodeIOstream <<
              ' ';
          } // for
        }

        fLilypondCodeIOstream <<
          ' ' <<
          dottedDurationAsLilypondString (
            inputLineNumber,
            tempoBeatUnit) <<
          " = " <<
          tempoPerMinute;

        fLilypondCodeIOstream << endl;
        break;
        */
      } // switch
      break;

    case msrTempo::kTempoBeatUnitsEquivalence:
      fLilypondCodeIOstream <<
        "\\tempo ";

      if (tempoWordsListSize) {
        list<S_msrWords>::const_iterator
          iBegin = tempoWordsList.begin (),
          iEnd   = tempoWordsList.end (),
          i      = iBegin;

        for ( ; ; ) {
          S_msrWords words = (*i);

          fLilypondCodeIOstream <<
            "\"" << words->getWordsContents () << "\"";

          if (++i == iEnd) break;

          fLilypondCodeIOstream <<
            ' ';
        } // for
      }

      fLilypondCodeIOstream <<
        ' ' <<
        "\\markup {" <<
        endl;

      gIndenter++;

      fLilypondCodeIOstream <<
        "\\concat {" <<
        endl;

      gIndenter++;

      switch (tempoParenthesizedKind) {
        case msrTempo::kTempoParenthesizedYes:
          fLilypondCodeIOstream <<
            "(" <<
            endl;
          break;
        case msrTempo::kTempoParenthesizedNo:
          break;
      } // switch

      gIndenter++;

      if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
        fLilypondCodeIOstream <<
          " \\smaller \\general-align #Y #DOWN \\note {";
      }
      else {
        fLilypondCodeIOstream <<
          " \\smaller \\general-align #Y #DOWN \\note #\"";
      }

      fLilypondCodeIOstream <<
        dottedDurationAsLilypondStringWithoutBackSlash (
          inputLineNumber,
          tempoBeatUnit);

      if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
        fLilypondCodeIOstream <<
          "} #UP";
      }
      else {
        fLilypondCodeIOstream <<
          "\" #UP";
      }

      fLilypondCodeIOstream <<
        endl <<
        "\" = \"" <<
        endl;

      fLilypondCodeIOstream <<
        "(";
      if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
        fLilypondCodeIOstream <<
          " \\smaller \\general-align #Y #DOWN \\note {";
      }
      else {
        fLilypondCodeIOstream <<
          " \\smaller \\general-align #Y #DOWN \\note #\"";
      }

      fLilypondCodeIOstream <<
        dottedDurationAsLilypondStringWithoutBackSlash (
          inputLineNumber,
          elt->getTempoEquivalentBeatUnit ());

      if (gLpsrOptions->versionNumberGreaterThanOrEqualTo ("2.20")) {
        fLilypondCodeIOstream <<
          "} #UP";
      }
      else {
        fLilypondCodeIOstream <<
          "\" #UP";
      }

      fLilypondCodeIOstream << endl;

      gIndenter--;

      switch (tempoParenthesizedKind) {
        case msrTempo::kTempoParenthesizedYes:
          fLilypondCodeIOstream <<
            ")" <<
            endl;
          break;
        case msrTempo::kTempoParenthesizedNo:
          break;
      } // switch

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;
      break;

    case msrTempo::kTempoNotesRelationShip:
      fLilypondCodeIOstream <<
        "\\tempoRelationship #\"";

      if (tempoWordsListSize) {
        list<S_msrWords>::const_iterator
          iBegin = tempoWordsList.begin (),
          iEnd   = tempoWordsList.end (),
          i      = iBegin;

        for ( ; ; ) {
          S_msrWords words = (*i);

          fLilypondCodeIOstream <<
     // JMI       "\"" <<
            words->getWordsContents (); // JMI <<
      // JMI      "\"";

          if (++i == iEnd) break;

          fLilypondCodeIOstream <<
            ' ';
        } // for
      }

      fLilypondCodeIOstream <<
        "\"";

      switch (tempoParenthesizedKind) {
        case msrTempo::kTempoParenthesizedYes:
          fLilypondCodeIOstream <<
            " ##t";
          break;
        case msrTempo::kTempoParenthesizedNo:
          fLilypondCodeIOstream <<
            " ##f";
          break;
      } // switch

      fLilypondCodeIOstream << endl;
      break;
  } // switch
}

void lpsr2LilypondTranslator::visitStart (S_msrTempoRelationshipElements& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTempoRelationshipElements" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\fixed b' {" <<
    endl;

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_msrTempoRelationshipElements& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTempoRelationshipElements" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  fLilypondCodeIOstream <<
    endl <<
    "}" <<
    endl;
}

void lpsr2LilypondTranslator::visitStart (S_msrTempoNote& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTempoNote" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "b" <<
    wholeNotesAsLilypondString (
      elt->getInputLineNumber (),
      elt->getTempoNoteWholeNotes ()) <<
      ' ';
}

void lpsr2LilypondTranslator::visitStart (S_msrTempoTuplet& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTempoTuplet" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\tuplet " <<
    elt->getTempoTupletActualNotes () <<
    "/" <<
    elt->getTempoTupletNormalNotes () << " { ";
}

void lpsr2LilypondTranslator::visitEnd (S_msrTempoTuplet& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTempoTuplet" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "}";
}

void lpsr2LilypondTranslator::visitEnd (S_msrTempo& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTempo" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrArticulation& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrArticulation" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the articulation here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrArticulation& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrArticulation" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrFermata& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrFermata" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/*
Articulations can be attached to rests as well as notes but they cannot be attached to multi-measure rests. A special predefined command, \fermataMarkup, is available for at- taching a fermata to a multi-measure rest (and only a multi-measure rest). This creates a MultiMeasureRestText object.
*/

/* JMI
  switch (elt->getArticulationPlacement ()) {
    case kPlacementNone:
      // nothing needed
      break;
    case kPlacementAbove:
      fLilypondCodeIOstream << "^";
      break;
    case kPlacementBelow:
      fLilypondCodeIOstream << "_";
      break;
  } // switch

  // don't generate fermatas for chord member notes
  if (false && fOnGoingNote) { // JMI
    switch (elt->getFermataTypeKind ()) {
      case msrFermata::kFermataTypeNone:
        // no placement needed
        break;
      case msrFermata::kFermataTypeUpright:
        // no placement needed
        break;
      case msrFermata::kFermataTypeInverted:
        fLilypondCodeIOstream << "_";
        break;
    } // switch

    switch (elt->getFermataKind ()) {
      case msrFermata::kNormalFermataKind:
        fLilypondCodeIOstream << "\\fermata ";
        break;
      case msrFermata::kAngledFermataKind:
        fLilypondCodeIOstream << "\\shortfermata ";
        break;
      case msrFermata::kSquareFermataKind:
        fLilypondCodeIOstream << "\\longfermata ";
        break;
    } // switch
  }
*/
}

void lpsr2LilypondTranslator::visitEnd (S_msrFermata& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrFermata" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrArpeggiato& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrArpeggiato" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/* VIRER JMI
  */
}

void lpsr2LilypondTranslator::visitEnd (S_msrArpeggiato& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrArpeggiato" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrNonArpeggiato& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrNonArpeggiato" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/* VIRER JMI
  */
}

void lpsr2LilypondTranslator::visitEnd (S_msrNonArpeggiato& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrNonArpeggiato" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTechnical& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTechnical" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the technical here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrTechnical& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTechnical" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTechnicalWithInteger& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTechnicalWithInteger" <<
      ", fOnGoingChord = " <<
      booleanAsString (fOnGoingChord) <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the technicalWithInteger here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrTechnicalWithInteger& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTechnicalWithInteger" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTechnicalWithFloat& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTechnicalWithFloat" <<
      ", fOnGoingChord = " <<
      booleanAsString (fOnGoingChord) <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the technicalWithFloat here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrTechnicalWithFloat& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTechnicalWithFloat" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTechnicalWithString& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTechnicalWithString" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the technicalWithString here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrTechnicalWithString& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTechnicalWithString" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrOrnament& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrOrnament" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the ornament here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrOrnament& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrOrnament" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrGlissando& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrGlissando" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the glissando here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrGlissando& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrGlissando" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrSlide& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrSlide" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the slide here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrSlide& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrSlide" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrSingleTremolo& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrSingleTremolo" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // don't generate the singleTremolo here,
  // the note or chord will do it in its visitEnd () method
}

void lpsr2LilypondTranslator::visitEnd (S_msrSingleTremolo& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrSingleTremolo" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrDoubleTremolo& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrDoubleTremolo" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // get double tremolo number of repeats
  int numberOfRepeats =
    elt->getDoubleTremoloNumberOfRepeats ();

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceTremolos) {
    fLilypondCodeIOstream <<
      "% visitStart (S_msrDoubleTremolo&)" <<
      endl;

    gIndenter++;

    fLilypondCodeIOstream <<
      "% doubleTremoloSoundingWholeNotes = " <<
      elt->getDoubleTremoloSoundingWholeNotes () <<
      endl <<

      "% gdoubleTremoloElementsDuration = " <<
      elt->getDoubleTremoloElementsDuration () <<
      endl <<

      "% doubleTremoloMarksNumber = " <<
      elt->getDoubleTremoloMarksNumber () <<
      endl <<

      "% numberOfRepeats = " <<
      numberOfRepeats <<
      endl;

    gIndenter++;
  }
#endif

  fLilypondCodeIOstream <<
    "\\repeat tremolo " << numberOfRepeats << " {";

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_msrDoubleTremolo& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrDoubleTremolo" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  fLilypondCodeIOstream <<
    "}" <<
    endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrDynamics& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrDynamics" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrDynamics& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrDynamics" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrOtherDynamics& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrOtherDynamics" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrOtherDynamics& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrOtherDynamics" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrWords& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrWords" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrWords& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrWords" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrSlur& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrSlur" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrSlur& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrSlur" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrLigature& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrLigature" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrLigature& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrLigature" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrWedge& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrWedge" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrWedge& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrWedge" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateNoteBeams (S_msrNote note)
{
  const list<S_msrBeam>&
    noteBeams =
      note->getNoteBeams ();

  if (noteBeams.size ()) {
    list<S_msrBeam>::const_iterator i;
    for (
      i=noteBeams.begin ();
      i!=noteBeams.end ();
      i++
    ) {
      S_msrBeam beam = (*i);

      // LilyPond will take care of multiple beams automatically,
      // so we need only generate code for the first number (level)
      switch (beam->getBeamKind ()) {

        case msrBeam::kBeginBeam:
          if (beam->getBeamNumber () == 1)
            fLilypondCodeIOstream << "[ ";
          break;

        case msrBeam::kContinueBeam:
          break;

        case msrBeam::kEndBeam:
          if (beam->getBeamNumber () == 1)
            fLilypondCodeIOstream << "] ";
          break;

        case msrBeam::kForwardHookBeam:
          break;

        case msrBeam::kBackwardHookBeam:
          break;

        case msrBeam::k_NoBeam:
          break;
      } // switch
    } // for
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateNoteSlurs (S_msrNote note)
{
  const list<S_msrSlur>&
    noteSlurs =
      note->getNoteSlurs ();

  if (noteSlurs.size ()) {
    list<S_msrSlur>::const_iterator i;
    for (
      i=noteSlurs.begin ();
      i!=noteSlurs.end ();
      i++
    ) {
      S_msrSlur slur = (*i);

      /*
      \slurDashed, \slurDotted, \slurHalfDashed,
      \slurHalfSolid, \slurDashPattern, \slurSolid
      */

      switch (slur->getSlurTypeKind ()) {
        case msrSlur::k_NoSlur:
          break;
        case msrSlur::kRegularSlurStart:
          fLilypondCodeIOstream << "( ";
          break;
        case msrSlur::kPhrasingSlurStart:
          fLilypondCodeIOstream << "\\( ";
          break;
        case msrSlur::kSlurContinue:
          break;
        case msrSlur::kRegularSlurStop:
          fLilypondCodeIOstream << ") ";
          break;
        case msrSlur::kPhrasingSlurStop:
          fLilypondCodeIOstream << "\\) ";
          break;
      } // switch
    } // for
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateGraceNotesGroup (
  S_msrGraceNotesGroup graceNotesGroup)
{
  /*
    1. no slash, no slur: \grace
    2. slash and slur: \acciaccatura
    3. slash but no slur: \slashedGrace
    4. no slash but slur: \appoggiatura
  */

  if (graceNotesGroup->getGraceNotesGroupIsSlashed ()) {
    if (graceNotesGroup->getGraceNotesGroupIsTied ()) {
      fLilypondCodeIOstream <<
        "\\acciaccatura";
    }
    else {
      fLilypondCodeIOstream <<
        "\\slashedGrace";
    }
  }

  else {
    if (graceNotesGroup->getGraceNotesGroupIsTied ()) {
      fLilypondCodeIOstream <<
        "\\appoggiatura";
    }
    else {
      fLilypondCodeIOstream <<
        "\\grace";
    }
  }

  fLilypondCodeIOstream <<
    " { ";

  // force durations to be displayed explicitly
  // at the beginning of the grace notes
  fLastMetWholeNotes = rational (0, 1);

  list<S_msrMeasureElement>&
    graceNotesGroupElementsList =
      graceNotesGroup->
        getGraceNotesGroupElementsList ();

  if (graceNotesGroupElementsList.size ()) {
    list<S_msrMeasureElement>::const_iterator
      iBegin = graceNotesGroupElementsList.begin (),
      iEnd   = graceNotesGroupElementsList.end (),
      i      = iBegin;

    for ( ; ; ) {
      S_msrElement element = (*i);

      if (
        // note?
        S_msrNote
          note =
            dynamic_cast<msrNote*>(&(*element))
        ) {
          // print things before the note
          generateCodeBeforeNote (note);

          // print the note itself
          generateCodeForNote (note);

          // print things after the note
          generateCodeAfterNote (note);

          // print the note beams if any,
          // unless the note is chord member
          if (! note->getNoteBelongsToAChord ()) {
            generateNoteBeams (note);
          }

          // print the note slurs if any,
          // unless the note is chord member
          if (! note->getNoteBelongsToAChord ()) {
            generateNoteSlurs (note);
          }
        }

      else if (
        // chord?
        S_msrChord
          chord =
            dynamic_cast<msrChord*>(&(*element))
        ) {
          generateChordInGraceNotesGroup (chord);
        }

      else {
        stringstream s;

        fLilypondCodeIOstream <<
          "grace notes group elements list in '" <<
          graceNotesGroup->asString () <<
          "' is empty" <<
          ", line " << graceNotesGroup->getInputLineNumber ();

        msrInternalError (
          gGeneralOptions->fInputSourceName,
          graceNotesGroup->getInputLineNumber (),
          __FILE__, __LINE__,
          s.str ());
      }

      if (++i == iEnd) break;
      fLilypondCodeIOstream <<
        ' ';
    } // for

    fLilypondCodeIOstream << "} ";
  }

  else {
    stringstream s;

    fLilypondCodeIOstream <<
      "grace notes group elements list in '" <<
      graceNotesGroup->asString () <<
      "' is empty" <<
      ", line " << graceNotesGroup->getInputLineNumber ();

    msrInternalError (
      gGeneralOptions->fInputSourceName,
      graceNotesGroup->getInputLineNumber (),
      __FILE__, __LINE__,
      s.str ());
  }

  // force durations to be displayed explicitly
  // at the end of the grace notes
  fLastMetWholeNotes = rational (0, 1);
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrGraceNotesGroup& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrGraceNotesGroup" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fOnGoingGraceNotesGroup = true;
}

void lpsr2LilypondTranslator::visitEnd (S_msrGraceNotesGroup& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrGraceNotesGroup" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fOnGoingGraceNotesGroup = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrAfterGraceNotesGroup& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrAfterGraceNotesGroup" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

 // JMI exists? if (elt->getGraceNotesGroupIsSlashed ()) {}
  fLilypondCodeIOstream <<
    "\\afterGrace { ";
}

void lpsr2LilypondTranslator::visitStart (S_msrAfterGraceNotesGroupContents& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrAfterGraceNotesGroupContents" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // write a first closing right bracket right now
  fLilypondCodeIOstream <<
    "} { ";

  // force durations to be displayed explicitly
  // at the beginning of the after grace notes contents
  fLastMetWholeNotes = rational (0, 1);
}

void lpsr2LilypondTranslator::visitEnd (S_msrAfterGraceNotesGroupContents& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrAfterGraceNotesGroupContents" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "} ";
}

void lpsr2LilypondTranslator::visitEnd (S_msrAfterGraceNotesGroup& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrAfterGraceNotesGroup" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrNote& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting " <<
      msrNote::noteKindAsString (elt->getNoteKind ()) <<
      " note" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // is this a rest measures to be ignored?
  switch (elt->getNoteKind ()) {
    case msrNote::kRestNote:
      // don't handle rest measuress, that's done in visitEnd (S_msrRestMeasures&)
      if (fOnGoingRestMeasures) {
        /*
        if (elt->getNoteOccupiesAFullMeasure ()) {
          bool inhibitRestMeasuresBrowsing =
            fVisitedLpsrScore->
              getMsrScore ()->
                getInhibitRestMeasuresBrowsing ();

          if (inhibitRestMeasuresBrowsing) {
            if (
              gMsrOptions->fTraceMsrVisitors
                ||
              gGeneralOptions->ffTraceRestMeasures) {
              gLogIOstream <<
                "% ==> visiting rest measures is ignored" <<
                endl;
            }

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesDetails) {
    gLogIOstream <<
      "% ==> returning from visitStart (S_msrNote&)" <<
    endl;
  }
#endif

          return;
          }
        }
        */

#ifdef TRACE_OPTIONS
        if (
          gMsrOptions->fTraceMsrVisitors
            ||
          gTraceOptions->fTraceRestMeasures
        ) {
          gLogIOstream <<
            "% ==> start visiting rest measures is ignored" <<
            endl;
        }
#endif

        return;
      }
      break;

    case msrNote::kSkipNote:
      if (elt->getNoteGraceNotesGroupUpLink ()) {
#ifdef TRACE_OPTIONS
        if (
          gMsrOptions->fTraceMsrVisitors
            ||
          gTraceOptions->fTraceNotes
        ) {
          gLogIOstream <<
            "% ==> start visiting skip notes is ignored" <<
            endl;
        }
#endif

          return;
      }
      break;

    case msrNote::kGraceNote:
#ifdef TRACE_OPTIONS
        if (
          gMsrOptions->fTraceMsrVisitors
            ||
          gTraceOptions->fTraceGraceNotes
        ) {
          gLogIOstream <<
            "% ==> start visiting grace notes is ignored" <<
            endl;
        }
#endif

        return;
      break;

    case msrNote::kGraceChordMemberNote:
#ifdef TRACE_OPTIONS
        if (
          gMsrOptions->fTraceMsrVisitors
            ||
          gTraceOptions->fTraceGraceNotes
        ) {
          gLogIOstream <<
            "% ==> start visiting chord grace notes is ignored" <<
            endl;
        }
#endif

        return;
      break;

    default:
      ;
  } // switch

  // get the note's grace notes after
  S_msrGraceNotesGroup
    noteGraceNotesGroupAfter =
      elt->getNoteGraceNotesGroupAfter ();

  // print the note's grace notes group after opener if any
  if (noteGraceNotesGroupAfter) {
    fLilypondCodeIOstream <<
      "\\afterGrace { ";
  }

  // print the note's grace notes before if any,
  // but not ??? JMI
  S_msrGraceNotesGroup
    noteGraceNotesGroupBefore =
      elt->getNoteGraceNotesGroupBefore ();

  if (noteGraceNotesGroupBefore) {
    generateGraceNotesGroup (
      noteGraceNotesGroupBefore);
  }

  // print the note scordaturas if any
  const list<S_msrScordatura>&
    noteScordaturas =
      elt->getNoteScordaturas ();

  if (noteScordaturas.size ()) {
    fLilypondCodeIOstream <<
      "<<" <<
      endl;

    gIndenter++;

    list<S_msrScordatura>::const_iterator
      iBegin = noteScordaturas.begin (),
      iEnd   = noteScordaturas.end (),
      i      = iBegin;
    for ( ; ; ) {
      S_msrScordatura
        scordatura = (*i);

      const list<S_msrStringTuning>&
        scordaturaStringTuningsList =
          scordatura->
            getScordaturaStringTuningsList ();

      gIndenter++;

      fLilypondCodeIOstream <<
        "\\new Staff \\with { alignAboveContext = \"" <<
        elt->getNoteMeasureUpLink ()->
          getMeasureSegmentUpLink ()->
            getSegmentVoiceUpLink ()->
              getVoiceStaffUpLink ()->
                getStaffName () <<
        "\" } {" <<
        endl;

      gIndenter++;

      fLilypondCodeIOstream <<
        "\\hide Staff.Stem" <<
        endl <<
        "\\hide Staff.TimeSignature" <<
        endl <<
        "\\small" <<
        endl <<
        "\\once \\override Score.RehearsalMark.self-alignment-X = #LEFT" <<
        endl <<
        "\\mark\\markup {\\small\\bold \"Scordatura\"}" <<
        endl <<
        "<";

      if (scordaturaStringTuningsList.size ()) {
        list<S_msrStringTuning>::const_iterator
          iBegin = scordaturaStringTuningsList.begin (),
          iEnd   = scordaturaStringTuningsList.end (),
          i      = iBegin;
        for ( ; ; ) {
          S_msrStringTuning
            stringTuning = (*i);

          fLilypondCodeIOstream <<
            stringTuningAsLilypondString (
              elt->getInputLineNumber (),
              stringTuning);

          if (++i == iEnd) break;

          fLilypondCodeIOstream << ' ';
        } // for
      }

      fLilypondCodeIOstream <<
        ">4" <<
        endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "}" <<
        endl;

      gIndenter--;

      fLilypondCodeIOstream <<
        "{" <<
        endl;

      gIndenter++;

      if (++i == iEnd) break;
    } // for
  }

  // should the note actually be printed?
  msrPrintObjectKind
    notePrintObjectKind =
      elt->getNotePrintObjectKind ();

  if (notePrintObjectKind != fCurrentNotePrinObjectKind) {
    switch (notePrintObjectKind) {
      case kPrintObjectNone:
        // JMI
        break;
      case kPrintObjectYes:
        fLilypondCodeIOstream <<
          endl <<
          "\\revert NoteHead.color" <<
          endl;
        break;
      case kPrintObjectNo:
        fLilypondCodeIOstream <<
          endl <<
          "\\temporary\\override NoteHead.color = #(rgb-color 0.5 0.5 0.5)" <<
          endl;
        break;
    } // switch

    fCurrentNotePrinObjectKind = notePrintObjectKind;
  }

  // print the note slashes if any
  const list<S_msrSlash>&
    noteSlashes =
      elt->getNoteSlashes ();

  if (noteSlashes.size ()) {
    list<S_msrSlash>::const_iterator i;
    for (
      i=noteSlashes.begin ();
      i!=noteSlashes.end ();
      i++
    ) {
      S_msrSlash
        slash =
          (*i);

      switch (slash->getSlashTypeKind ()) {
        case k_NoSlashType:
          break;

        case kSlashTypeStart:
          fLilypondCodeIOstream <<
            endl <<
            "\\override Staff.NoteHead.style = #'slash " <<
            endl;
          break;

        case kSlashTypeStop:
          fLilypondCodeIOstream <<
            endl <<
            "\\revert Staff.NoteHead.style " <<
            endl;
          break;
      } // switch

      switch (slash->getSlashUseDotsKind ()) {
        case k_NoSlashUseDots:
          break;

        case kSlashUseDotsYes:
          fLilypondCodeIOstream <<
            endl <<
            "\\override Staff.NoteHead.style = #'slash " <<
            endl;
          break;

        case kSlashUseDotsNo:
          fLilypondCodeIOstream <<
            endl <<
            "\\revert Staff.NoteHead.style " <<
            endl;
          break;
      } // switch

      switch (slash->getSlashUseStemsKind ()) {
        case k_NoSlashUseStems:
          break;

        case kSlashUseStemsYes:
          fLilypondCodeIOstream <<
            endl <<
            "\\undo \\hide Staff.Stem " <<
            endl;
          break;

        case kSlashUseStemsNo:
          fLilypondCodeIOstream <<
            endl <<
            "\\hide Staff.Stem " <<
            endl;
          break;
      } // switch
    } // for
  }

  // print the note wedges circled tips if any
  const list<S_msrWedge>&
    noteWedges =
      elt->getNoteWedges ();

  if (noteWedges.size ()) {
    list<S_msrWedge>::const_iterator i;
    for (
      i=noteWedges.begin ();
      i!=noteWedges.end ();
      i++
    ) {
      S_msrWedge wedge = (*i);

      switch (wedge->getWedgeKind ()) {
        case msrWedge::kWedgeKindNone:
          break;

        case msrWedge::kWedgeCrescendo:
          switch (wedge->getWedgeNienteKind ()) {
            case msrWedge::kWedgeNienteYes:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Hairpin.circled-tip = ##t " <<
                endl;
              break;
            case msrWedge::kWedgeNienteNo:
              break;
            } // switch
          break;

        case msrWedge::kWedgeDecrescendo:
          switch (wedge->getWedgeNienteKind ()) {
            case msrWedge::kWedgeNienteYes:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Hairpin.circled-tip = ##t " <<
                endl;
              break;
            case msrWedge::kWedgeNienteNo:
              break;
            } // switch
          break;

        case msrWedge::kWedgeStop:
        /* JMI
          fLilypondCodeIOstream <<
            "\\! ";
            */
          break;
      } // switch
    } // for
  }

  // print the note slurs line types if any,
  // unless the note is chord member
  if (! elt->getNoteBelongsToAChord ()) {
    const list<S_msrSlur>&
      noteSlurs =
        elt->getNoteSlurs ();

    if (noteSlurs.size ()) {
      list<S_msrSlur>::const_iterator i;
      for (
        i=noteSlurs.begin ();
        i!=noteSlurs.end ();
        i++
      ) {
        S_msrSlur slur = (*i);

        /*
        \slurDashed, \slurDotted, \slurHalfDashed,
        \slurHalfSolid, \slurDashPattern, \slurSolid
        */

        switch (slur->getSlurTypeKind ()) {
          case msrSlur::kRegularSlurStart:
          case msrSlur::kPhrasingSlurStart:
            switch (slur->getSlurLineTypeKind ()) {
              case kLineTypeSolid:
                /* JMI ???
                fLilypondCodeIOstream <<
                  "\\once\\slurSolid ";
                */
                break;
              case kLineTypeDashed:
                fLilypondCodeIOstream <<
                  "\\once\\slurDashed ";
                break;
              case kLineTypeDotted:
                fLilypondCodeIOstream <<
                  "\\once\\slurDotted ";
                break;
              case kLineTypeWavy:
                fLilypondCodeIOstream <<
                  "\\once\\slurWavy "; // JMI
                break;
            } // switch
            break;
          default:
            ;
        } // switch
      } // for
    }
  }

  // print the note glissandos styles if any
  const list<S_msrGlissando>&
    noteGlissandos =
      elt->getNoteGlissandos ();

  if (noteGlissandos.size ()) {
    list<S_msrGlissando>::const_iterator i;
    for (
      i=noteGlissandos.begin ();
      i!=noteGlissandos.end ();
      i++
    ) {
      S_msrGlissando glissando = (*i);

      switch (glissando->getGlissandoTypeKind ()) {
        case msrGlissando::kGlissandoTypeNone:
          break;

        case msrGlissando::kGlissandoTypeStart:
          // generate the glissando style
          switch (glissando->getGlissandoLineTypeKind ()) {
            case kLineTypeSolid:
              break;
            case kLineTypeDashed:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.style = #'dashed-line" <<
                endl;
              break;
            case kLineTypeDotted:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.style = #'dotted-line" <<
                endl;
              break;
            case kLineTypeWavy:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.style = #'zigzag" <<
                endl;
              break;
          } // switch
          break;

        case msrGlissando::kGlissandoTypeStop:
          break;
      } // switch
    } // for
  }

  // print the note slides styles if any, implemented as glissandos
  const list<S_msrSlide>&
    noteSlides =
      elt->getNoteSlides ();

  if (noteSlides.size ()) {
    list<S_msrSlide>::const_iterator i;
    for (
      i=noteSlides.begin ();
      i!=noteSlides.end ();
      i++
    ) {
      S_msrSlide slide = (*i);

      switch (slide->getSlideTypeKind ()) {
        case msrSlide::kSlideTypeNone:
          break;

        case msrSlide::kSlideTypeStart:
          // generate the glissando style
          switch (slide->getSlideLineTypeKind ()) {
            case kLineTypeSolid:
              break;
            case kLineTypeDashed:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.style = #'dashed-line" <<
                endl;
              break;
            case kLineTypeDotted:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.style = #'dotted-line" <<
                endl;
              break;
            case kLineTypeWavy:
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.style = #'zigzag" <<
                endl;
              break;
          } // switch
          break;

        case msrSlide::kSlideTypeStop:
          break;
      } // switch
    } // for
  }

  // print the note glissandos with text if any
  if (noteGlissandos.size ()) {
    list<S_msrGlissando>::const_iterator i;
    for (
      i=noteGlissandos.begin ();
      i!=noteGlissandos.end ();
      i++
    ) {
      S_msrGlissando glissando = (*i);

      switch (glissando->getGlissandoTypeKind ()) {
        case msrGlissando::kGlissandoTypeNone:
          break;

        case msrGlissando::kGlissandoTypeStart:
          {
            string
              glissandoTextValue =
                glissando->getGlissandoTextValue ();

            if (glissandoTextValue.size ()) {
              // generate the glissando text on itself
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.details.glissando-text = \"" <<
                glissandoTextValue <<
                "\"" <<
                endl <<
                "\\glissandoTextOn" <<
                endl;
            }
          }
          break;

        case msrGlissando::kGlissandoTypeStop:
          break;
      } // switch
    } // for
  }

  // print the note slides with text if any
  if (noteSlides.size ()) {
    list<S_msrSlide>::const_iterator i;
    for (
      i=noteSlides.begin ();
      i!=noteSlides.end ();
      i++
    ) {
      S_msrSlide slide = (*i);

      switch (slide->getSlideTypeKind ()) {
        case msrSlide::kSlideTypeNone:
          break;

        case msrSlide::kSlideTypeStart:
          {
            string
              slideTextValue =
                slide->getSlideTextValue ();

            if (slideTextValue.size ()) {
              // generate the slide text on itself
              fLilypondCodeIOstream <<
                endl <<
                "\\once\\override Glissando.details.glissando-text = \"" <<
                slideTextValue <<
                "\"" <<
                endl <<
                "\\glissandoTextOn" <<
                endl;
            }
          }
          break;

        case msrSlide::kSlideTypeStop:
          break;
      } // switch
    } // for
  }

  // print the note spanners if any
  const list<S_msrSpanner>&
    noteSpanners =
      elt->getNoteSpanners ();

  if (noteSpanners.size ()) {
    list<S_msrSpanner>::const_iterator i;
    for (
      i=noteSpanners.begin ();
      i!=noteSpanners.end ();
      i++
    ) {
      S_msrSpanner
        spanner = (*i);

      bool doGenerateSpannerCode = true;

      switch (spanner->getSpannerKind ()) {
        case msrSpanner::kSpannerDashes:
          break;
        case msrSpanner::kSpannerWavyLine:
          if (spanner->getSpannerNoteUpLink ()->getNoteTrillOrnament ()) {
            // don't generate anything, the trill will display the wavy line
            doGenerateSpannerCode = false;
          }
          break;
      } // switch

      if (doGenerateSpannerCode) {
        switch (spanner->getSpannerKind ()) {
          case msrSpanner::kSpannerDashes:
          case msrSpanner::kSpannerWavyLine:
            break;
        } // switch

        generateCodeForSpannerBeforeNote (spanner);
      }
    } // for
  }

  // should the note be parenthesized?
  msrNote::msrNoteHeadParenthesesKind
    noteHeadParenthesesKind =
      elt->getNoteHeadParenthesesKind ();

  switch (noteHeadParenthesesKind) {
    case msrNote::kNoteHeadParenthesesYes:
      fLilypondCodeIOstream << "\\parenthesize ";
      break;
    case msrNote::kNoteHeadParenthesesNo:
      break;
  } // switch

  // print the note technicals with string if any
  const list<S_msrTechnicalWithString>&
    noteTechnicalWithStrings =
      elt->getNoteTechnicalWithStrings ();

  if (noteTechnicalWithStrings.size ()) {
    list<S_msrTechnicalWithString>::const_iterator i;
    for (
      i=noteTechnicalWithStrings.begin ();
      i!=noteTechnicalWithStrings.end ();
      i++
    ) {
      S_msrTechnicalWithString technicalWithString = (*i);

      switch (technicalWithString->getTechnicalWithStringKind ()) {
        case msrTechnicalWithString::kHammerOn:
          switch (technicalWithString->getTechnicalWithStringTypeKind ()) {
            case kTechnicalTypeStart:
              {
                rational
                  noteSoundingWholeNotes =
                    elt->getNoteSoundingWholeNotes ();

                rational
                  halfWholeNotes =
                    noteSoundingWholeNotes /2;

                fLilypondCodeIOstream <<
                  "\\after " <<
                  wholeNotesAsLilypondString (
                    elt->getInputLineNumber (),
                    halfWholeNotes) <<
                  " ^\"H\" ";
              }
              break;
            case kTechnicalTypeStop:
              break;
            case k_NoTechnicalType:
              break;
          } // switch
          break;
        case msrTechnicalWithString::kHandbell: // JMI
          break;
        case msrTechnicalWithString::kOtherTechnical: // JMI
          break;
        case msrTechnicalWithString::kPluck: // JMI
          break;
        case msrTechnicalWithString::kPullOff:
          switch (technicalWithString->getTechnicalWithStringTypeKind ()) {
            case kTechnicalTypeStart:
              {
                rational
                  noteSoundingWholeNotes =
                    elt->getNoteSoundingWholeNotes ();

                rational
                  halfWholeNotes =
                    noteSoundingWholeNotes /2;

                fLilypondCodeIOstream <<
                  "\\after " <<
                  wholeNotesAsLilypondString (
                    elt->getInputLineNumber (),
                    halfWholeNotes) <<
                  " ^\"P\" ";
              }
              break;
            case kTechnicalTypeStop:
              break;
            case k_NoTechnicalType:
              break;
          } // switch
          break;
      } // switch
    } // for
  }

  // is the note a cue note?
  if (elt->getNoteIsACueNote ()) {
    fLilypondCodeIOstream <<
      "\\once \\override NoteHead.font-size = -3 ";
  }

  // has the note an octave shift up or down?
  if (! fOnGoingChord) {
    // the octave shift for the chords has already been generated
    S_msrOctaveShift
      noteOctaveShift =
        elt->
          getNoteOctaveShift ();

    if (noteOctaveShift) {
      generateCodeForOctaveShift (
        noteOctaveShift);
    }
  }

  // print things before the note
  generateCodeBeforeNote (elt);

  ////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////
  // print the note itself as a LilyPond string
  ////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////

  // print the note itself
  generateCodeForNote (elt);

  // print things after the note
  generateCodeAfterNote (elt);

  if (
    gLilypondOptions->fInputLineNumbers
      ||
    gLilypondOptions->fPositionsInMeasures
  ) {
    generateInputLineNumberAndOrPositionInMeasureAsAComment (
      elt);
  }

  fOnGoingNote = true;
}

void lpsr2LilypondTranslator::visitEnd (S_msrNote& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting " <<
      msrNote::noteKindAsString (elt->getNoteKind ()) <<
      " note" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  // is this a rest measures to be ignored?
  switch (elt->getNoteKind ()) {
    case msrNote::kRestNote:
      // don't handle rest measuress, that's done in visitEnd (S_msrRestMeasures&)
      if (fOnGoingRestMeasures) {
        if (elt->getNoteOccupiesAFullMeasure ()) {
          bool inhibitRestMeasuresBrowsing =
            fVisitedLpsrScore->
              getMsrScore ()->
                getInhibitRestMeasuresBrowsing ();

          if (inhibitRestMeasuresBrowsing) {
#ifdef TRACE_OPTIONS
            if (
              gTraceOptions->fTraceNotes
                ||
              gTraceOptions->fTraceRestMeasures
            ) {
              gLogIOstream <<
                "% ==> end visiting rest measures is ignored" <<
                endl;
            }
#endif

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceNotesDetails) {
    gLogIOstream <<
      "% ==> returning from visitEnd (S_msrNote&)" <<
      endl;
  }
#endif

          return;
          }
        }
      }
      break;

    case msrNote::kSkipNote:
      if (elt->getNoteGraceNotesGroupUpLink ()) {
#ifdef TRACE_OPTIONS
        if (
          gMsrOptions->fTraceMsrVisitors
            ||
          gTraceOptions->fTraceNotes
        ) {
          gLogIOstream <<
            "% ==> end visiting skip notes is ignored" <<
            endl;
        }
#endif

        return;
      }
      break;

    case msrNote::kGraceNote:
#ifdef TRACE_OPTIONS
        if (
          gMsrOptions->fTraceMsrVisitors
            ||
          gTraceOptions->fTraceGraceNotes) {
          gLogIOstream <<
            "% ==> end visiting grace notes is ignored" <<
            endl;
        }
#endif

        return;
      break;

    default:
      ;
  } // switch

  // fetch the note single tremolo
  S_msrSingleTremolo
    noteSingleTremolo =
      elt->getNoteSingleTremolo ();

  if (noteSingleTremolo) {
    // generate code for the single tremolo only
    // if note doesn't belong to a chord,
    // otherwise it will be generated for the chord itself
    if (! elt->getNoteBelongsToAChord ()) {
      fLilypondCodeIOstream <<
        singleTremoloDurationAsLilypondString (
          noteSingleTremolo);
    }
  }

  // print the note words if any,
  // which should precede the articulations in LilyPond
  // generate code for the words only
  // if note doesn't belong to a chord,
  // otherwise it will be generated for the chord itself
  if (! elt->getNoteBelongsToAChord ()) {
    const list<S_msrWords>&
      noteWords =
        elt->getNoteWords ();

    if (noteWords.size ()) {
      list<S_msrWords>::const_iterator i;
      for (
        i=noteWords.begin ();
        i!=noteWords.end ();
        i++
      ) {
        msrPlacementKind
          wordsPlacementKind =
            (*i)->getWordsPlacementKind ();

        string wordsContents =
          (*i)->getWordsContents ();

        msrFontStyleKind
          wordsFontStyleKind =
            (*i)->getWordsFontStyleKind ();

        S_msrFontSize
          wordsFontSize =
            (*i)->getWordsFontSize ();

        msrFontWeightKind
          wordsFontWeightKind =
            (*i)->getWordsFontWeightKind ();

        string markup;

        {
          // create markup apart to have its length available
          stringstream s;

          switch (wordsPlacementKind) {
            case kPlacementNone:
              s << "^";
              break;
            case kPlacementAbove:
              s << "^";
              break;
            case kPlacementBelow:
              s << "_";
              break;
          } // switch

          s <<
            "\\markup" << " { ";

          switch (wordsFontStyleKind) {
            case kFontStyleNone:
              break;
            case kFontStyleNormal:
              // LilyPond produces 'normal style' text by default
              break;
            case KFontStyleItalic:
              s <<
                "\\italic ";
              break;
          } // switch

          switch (wordsFontWeightKind) {
            case kFontWeightNone:
              break;
            case kFontWeightNormal:
              // LilyPond produces 'normal weight' text by default
              break;
            case kFontWeightBold:
              s <<
                "\\bold ";
              break;
          } // switch

          switch (wordsFontSize->getFontSizeKind ()) {
            case msrFontSize::kFontSizeNone:
              break;
            case msrFontSize::kFontSizeXXSmall:
              s <<
                "\\tiny ";
              break;
            case msrFontSize::kFontSizeXSmall:
              s <<
                "\\smaller ";
              break;
            case msrFontSize::kFontSizeSmall:
              s <<
                "\\small ";
              break;
            case msrFontSize::kFontSizeMedium:
              s <<
                "\\normalsize ";
              break;
            case msrFontSize::kFontSizeLarge:
              s <<
                "\\large ";
              break;
            case msrFontSize::kFontSizeXLarge:
              s <<
                "\\larger ";
              break;
            case msrFontSize::kFontSizeXXLarge:
              s <<
                "\\huge ";
              break;
            case msrFontSize::kFontSizeNumeric:
            /* JMI
              s <<
                "%{ " <<
                wordsFontSize->getFontNumericSize () <<
                " points %} ";
                */
              break;
          } // switch

          s <<
   // JMI         quoteStringIfNonAlpha (wordsContents) <<
            "\"" << wordsContents << "\"" <<
            " } ";

          markup = s.str ();
          }

        fLilypondCodeIOstream <<
          markup;
      } // for
    }
  }

/* TOO EARLY FOR ALL OF THEM??? JMI
  // print the note articulations if any
  if (! fOnGoingChord) {
    const list<S_msrArticulation>&
      noteArticulations =
        elt->getNoteArticulations ();

    if (noteArticulations.size ()) {
      list<S_msrArticulation>::const_iterator i;
      for (
        i=noteArticulations.begin ();
        i!=noteArticulations.end ();
        i++
      ) {
        S_msrArticulation articulation = (*i);
        switch (articulation->getArticulationKind ()) {
          case msrArticulation::kFermata: // handle this better JMI
            if (
              // fermata?
              S_msrFermata
                fermata =
                  dynamic_cast<msrFermata*>(&(*articulation))
              ) {
              switch (fermata->getFermataTypeKind ()) {
                case msrFermata::kFermataTypeNone:
                  // no placement needed
                  break;
                case msrFermata::kFermataTypeUpright:
                  // no placement needed
                  break;
                case msrFermata::kFermataTypeInverted:
                  fLilypondCodeIOstream << "_";
                  break;
              } // switch

              switch (fermata->getFermataKind ()) {
                case msrFermata::kNormalFermataKind:
                  fLilypondCodeIOstream << "\\fermata ";
                  break;
                case msrFermata::kAngledFermataKind:
                  fLilypondCodeIOstream << "\\shortfermata ";
                  break;
                case msrFermata::kSquareFermataKind:
                  fLilypondCodeIOstream << "\\longfermata ";
                  break;
              } // switch
            }
            else {
              stringstream s;

              s <<
                "note articulation '" <<
                articulation->asString () <<
                "' has 'fermata' kind, but is not of type S_msrFermata" <<
                ", line " << articulation->getInputLineNumber ();

              msrInternalError (
                gGeneralOptions->fInputSourceName,
                articulation->getInputLineNumber (),
                __FILE__, __LINE__,
                s.str ());
            }
            break;

          default:
            generateNoteArticulation ((*i));
            fLilypondCodeIOstream <<
              ' ';
        } // switch
      } // for
    }
  }
  */

  // print the note technicals if any
  const list<S_msrTechnical>&
    noteTechnicals =
      elt->getNoteTechnicals ();

  if (noteTechnicals.size ()) {
    list<S_msrTechnical>::const_iterator i;
    for (
      i=noteTechnicals.begin ();
      i!=noteTechnicals.end ();
      i++
    ) {

      fLilypondCodeIOstream <<
        technicalAsLilypondString ((*i));

      switch ((*i)->getTechnicalPlacementKind ()) {
        case kPlacementNone:
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          fLilypondCodeIOstream << "_";
          break;
      } // switch

      fLilypondCodeIOstream << ' ';
    } // for
  }

  // print the note technicals with integer if any,
  // but not for chord member notes strings:
  // they should appear after the chord itself
  switch (elt->getNoteKind ()) {
    case msrNote::kChordMemberNote:
       break;

    default:
      {
        const list<S_msrTechnicalWithInteger>&
          noteTechnicalWithIntegers =
            elt->getNoteTechnicalWithIntegers ();

        if (noteTechnicalWithIntegers.size ()) {
          list<S_msrTechnicalWithInteger>::const_iterator i;
          for (
            i=noteTechnicalWithIntegers.begin ();
            i!=noteTechnicalWithIntegers.end ();
            i++
          ) {

            S_msrTechnicalWithInteger
                technicalWithInteger = (*i);

            fLilypondCodeIOstream <<
              technicalWithIntegerAsLilypondString (
                technicalWithInteger);

            switch (technicalWithInteger->getTechnicalWithIntegerPlacementKind ()) {
              case kPlacementNone:
                break;
              case kPlacementAbove:
                fLilypondCodeIOstream << "^";
                break;
              case kPlacementBelow:
                fLilypondCodeIOstream << "_";
                break;
            } // switch

            fLilypondCodeIOstream << ' ';
          } // for
        }
      }
  } // switch

  // print the note technicals with float if any,
  // but not for chord member notes strings:
  // they should appear after the chord itself
  switch (elt->getNoteKind ()) {
    case msrNote::kChordMemberNote:
       break;

    default:
      {
        const list<S_msrTechnicalWithFloat>&
          noteTechnicalWithFloats =
            elt->getNoteTechnicalWithFloats ();

        if (noteTechnicalWithFloats.size ()) {
          list<S_msrTechnicalWithFloat>::const_iterator i;
          for (
            i=noteTechnicalWithFloats.begin ();
            i!=noteTechnicalWithFloats.end ();
            i++
          ) {

            S_msrTechnicalWithFloat
                technicalWithFloat = (*i);

            fLilypondCodeIOstream <<
              technicalWithFloatAsLilypondString (
                technicalWithFloat);

            switch (technicalWithFloat->getTechnicalWithFloatPlacementKind ()) {
              case kPlacementNone:
                break;
              case kPlacementAbove:
                fLilypondCodeIOstream << "^";
                break;
              case kPlacementBelow:
                fLilypondCodeIOstream << "_";
                break;
            } // switch

            fLilypondCodeIOstream << ' ';
          } // for
        }
      }
  } // switch

  // print the note technicals with string if any
  const list<S_msrTechnicalWithString>&
    noteTechnicalWithStrings =
      elt->getNoteTechnicalWithStrings ();

  if (noteTechnicalWithStrings.size ()) {
    list<S_msrTechnicalWithString>::const_iterator i;
    for (
      i=noteTechnicalWithStrings.begin ();
      i!=noteTechnicalWithStrings.end ();
      i++
    ) {

      fLilypondCodeIOstream <<
        technicalWithStringAsLilypondString ((*i));

      switch ((*i)->getTechnicalWithStringPlacementKind ()) {
        case kPlacementNone:
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          fLilypondCodeIOstream << "_";
          break;
      } // switch

      fLilypondCodeIOstream << ' ';
    } // for
  }

  // print the note ornaments if any
  list<S_msrOrnament>
    noteOrnaments =
      elt->getNoteOrnaments ();

  if (noteOrnaments.size ()) {
    list<S_msrOrnament>::const_iterator i;
    for (
      i=noteOrnaments.begin ();
      i!=noteOrnaments.end ();
      i++
    ) {
      S_msrOrnament
        ornament = (*i);

      generateOrnament (ornament); // some ornaments are not yet supported
    } // for
  }

  // print the note dynamics if any
  if (! fOnGoingChord) {
    const list<S_msrDynamics>&
      noteDynamics =
        elt->getNoteDynamics ();

    if (noteDynamics.size ()) {
      list<S_msrDynamics>::const_iterator i;
      for (
        i=noteDynamics.begin ();
        i!=noteDynamics.end ();
        i++
      ) {
        S_msrDynamics
          dynamics = (*i);

        switch (dynamics->getDynamicsPlacementKind ()) {
          case kPlacementNone:
     // JMI       fLilypondCodeIOstream << "-3";
            break;
          case kPlacementAbove:
            fLilypondCodeIOstream << "^";
            break;
          case kPlacementBelow:
            // this is done by LilyPond by default
            break;
        } // switch

        fLilypondCodeIOstream <<
          dynamicsAsLilypondString (dynamics) << ' ';
      } // for
    }
  }

  // print the note other dynamics if any
  if (! fOnGoingChord) {
    const list<S_msrOtherDynamics>&
      noteOtherDynamics =
        elt->getNoteOtherDynamics ();

    if (noteOtherDynamics.size ()) {
      list<S_msrOtherDynamics>::const_iterator i;
      for (
        i=noteOtherDynamics.begin ();
        i!=noteOtherDynamics.end ();
        i++
      ) {
        fLilypondCodeIOstream <<
          "-\\markup { "
          "\\dynamic \"" << (*i)->getOtherDynamicsString () << "\" } ";
      } // for
    }
  }

  // print the note beams if any,
  // unless the note is chord member
  if (! elt->getNoteBelongsToAChord ()) {
    generateNoteBeams (elt);
  }

  // print the note slurs if any,
  // unless the note is chord member
  if (! elt->getNoteBelongsToAChord ()) {
    generateNoteSlurs (elt);
  }

  // print the note ligatures if any
  const list<S_msrLigature>&
    noteLigatures =
      elt->getNoteLigatures ();

  if (noteLigatures.size ()) {
    list<S_msrLigature>::const_iterator i;
    for (
      i=noteLigatures.begin ();
      i!=noteLigatures.end ();
      i++
    ) {

      switch ((*i)->getLigatureKind ()) {
        case msrLigature::kLigatureNone:
          break;
        case msrLigature::kLigatureStart:
   // JMI       fLilypondCodeIOstream << "\\[ ";
          break;
        case msrLigature::kLigatureContinue:
          break;
        case msrLigature::kLigatureStop:
          fLilypondCodeIOstream << "\\] ";
          break;
      } // switch
    } // for
  }

  // print the note wedges if any
  const list<S_msrWedge>&
    noteWedges =
      elt->getNoteWedges ();

  if (noteWedges.size ()) {
    list<S_msrWedge>::const_iterator i;
    for (
      i=noteWedges.begin ();
      i!=noteWedges.end ();
      i++
    ) {
      S_msrWedge wedge = (*i);

      switch (wedge->getWedgeKind ()) {
        case msrWedge::kWedgeKindNone:
          break;

        case msrWedge::kWedgeCrescendo:
          switch (wedge->getWedgePlacementKind ()) {
            case kPlacementNone:
              break;
            case kPlacementAbove:
              fLilypondCodeIOstream <<
                "^";
              break;
            case kPlacementBelow:
              fLilypondCodeIOstream <<
                "_";
              break;
            } // switch
          fLilypondCodeIOstream <<
            "\\< ";
          break;

        case msrWedge::kWedgeDecrescendo:
          switch (wedge->getWedgePlacementKind ()) {
            case kPlacementNone:
              break;
            case kPlacementAbove:
              fLilypondCodeIOstream <<
                "^";
              break;
            case kPlacementBelow:
              fLilypondCodeIOstream <<
                "_";
              break;
            } // switch
          fLilypondCodeIOstream <<
            "\\> ";
          break;

        case msrWedge::kWedgeStop:
          fLilypondCodeIOstream <<
            "\\! ";
          break;
      } // switch
    } // for
  }

  // print the note articulations if any,
  // which should follo the dynamics and wedges in LilyPond
  if (! fOnGoingChord) {
    const list<S_msrArticulation>&
      noteArticulations =
        elt->getNoteArticulations ();

    if (noteArticulations.size ()) {
      list<S_msrArticulation>::const_iterator i;
      for (
        i=noteArticulations.begin ();
        i!=noteArticulations.end ();
        i++
      ) {
        S_msrArticulation articulation = (*i);
        switch (articulation->getArticulationKind ()) {
          case msrArticulation::kFermata: // handle this better JMI
            if (
              // fermata?
              S_msrFermata
                fermata =
                  dynamic_cast<msrFermata*>(&(*articulation))
              ) {
              switch (fermata->getFermataTypeKind ()) {
                case msrFermata::kFermataTypeNone:
                  // no placement needed
                  break;
                case msrFermata::kFermataTypeUpright:
                  // no placement needed
                  break;
                case msrFermata::kFermataTypeInverted:
                  fLilypondCodeIOstream << "_";
                  break;
              } // switch

              switch (fermata->getFermataKind ()) {
                case msrFermata::kNormalFermataKind:
                  fLilypondCodeIOstream << "\\fermata ";
                  break;
                case msrFermata::kAngledFermataKind:
                  fLilypondCodeIOstream << "\\shortfermata ";
                  break;
                case msrFermata::kSquareFermataKind:
                  fLilypondCodeIOstream << "\\longfermata ";
                  break;
              } // switch
            }
            else {
              stringstream s;

              s <<
                "note articulation '" <<
                articulation->asString () <<
                "' has 'fermata' kind, but is not of type S_msrFermata" <<
                ", line " << articulation->getInputLineNumber ();

              msrInternalError (
                gGeneralOptions->fInputSourceName,
                articulation->getInputLineNumber (),
                __FILE__, __LINE__,
                s.str ());
            }
            break;

          default:
            generateNoteArticulation ((*i));
            fLilypondCodeIOstream <<
              " ";
        } // switch
      } // for
    }
  }

  // print the note glissandos if any
  const list<S_msrGlissando>&
    noteGlissandos =
      elt->getNoteGlissandos ();

  if (noteGlissandos.size ()) {
    list<S_msrGlissando>::const_iterator i;
    for (
      i=noteGlissandos.begin ();
      i!=noteGlissandos.end ();
      i++
    ) {
      S_msrGlissando glissando = (*i);

      switch (glissando->getGlissandoTypeKind ()) {
        case msrGlissando::kGlissandoTypeNone:
          break;

        case msrGlissando::kGlissandoTypeStart:
          // generate the glissando itself
          fLilypondCodeIOstream <<
            "\\glissando " <<
            "\\glissandoTextOff ";
          break;

        case msrGlissando::kGlissandoTypeStop:
          break;
      } // switch
    } // for
  }

  // print the note slides if any, implemented as glissandos
  const list<S_msrSlide>&
    noteSlides =
      elt->getNoteSlides ();

  if (noteSlides.size ()) {
    list<S_msrSlide>::const_iterator i;
    for (
      i=noteSlides.begin ();
      i!=noteSlides.end ();
      i++
    ) {
      S_msrSlide slide = (*i);

      switch (slide->getSlideTypeKind ()) {
        case msrSlide::kSlideTypeNone:
          break;

        case msrSlide::kSlideTypeStart:
          // generate the glissando itself
          fLilypondCodeIOstream <<
            "\\glissando " <<
            "\\glissandoTextOff ";
          break;

        case msrSlide::kSlideTypeStop:
          break;
      } // switch
    } // for
  }

  // print the note spanners if any
  const list<S_msrSpanner>&
    noteSpanners =
      elt->getNoteSpanners ();

  if (noteSpanners.size ()) {
    list<S_msrSpanner>::const_iterator i;
    for (
      i=noteSpanners.begin ();
      i!=noteSpanners.end ();
      i++
    ) {
      S_msrSpanner
        spanner = (*i);

      bool doGenerateSpannerCode = true;

      switch (spanner->getSpannerKind ()) {
        case msrSpanner::kSpannerDashes:
          break;
        case msrSpanner::kSpannerWavyLine:
          if (spanner->getSpannerNoteUpLink ()->getNoteTrillOrnament ()) {
            // don't generate anything, the trill will display the wavy line
            doGenerateSpannerCode = false;
          }
          break;
      } // switch

      if (doGenerateSpannerCode) {
        switch (spanner->getSpannerPlacementKind ()) {
          case kPlacementNone:
     // JMI       fLilypondCodeIOstream << "-3";
            break;
          case kPlacementAbove:
            fLilypondCodeIOstream << "^";
            break;
          case kPlacementBelow:
            // this is done by LilyPond by default
            break;
        } // switch

        generateCodeForSpannerAfterNote (spanner);
      }
    } // for
  }

  // are there note scordaturas?
  const list<S_msrScordatura>&
    noteScordaturas =
      elt->getNoteScordaturas ();

  if (noteScordaturas.size ()) {
    gIndenter--;

    fLilypondCodeIOstream <<
      endl <<
      "}" <<
      endl;

    gIndenter--;

    fLilypondCodeIOstream <<
      ">>" <<
      endl;
  }

  if (false && elt->getNoteIsFollowedByGraceNotesGroup ()) { // JMI
    if (! elt->getNoteIsARest ()) {
      fLilypondCodeIOstream <<
       " { % NoteIsFollowedByGraceNotesGroup" <<
        endl; // JMI ???
    }
  }

  // get the note's grace notes after
  S_msrGraceNotesGroup
    noteGraceNotesGroupAfter =
      elt->getNoteGraceNotesGroupAfter ();

  // print the note's grace notes after group closer if any
  if (noteGraceNotesGroupAfter) {
    fLilypondCodeIOstream <<
      "} { ";
    generateGraceNotesGroup (
      noteGraceNotesGroupAfter);
    fLilypondCodeIOstream <<
      "} ";
  }

  fOnGoingNote = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrOctaveShift& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrOctaveShift" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrOctaveShift& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrOctaveShift" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrAccordionRegistration& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrAccordionRegistration" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int highDotsNumber =
    elt->getHighDotsNumber ();
  int middleDotsNumber =
    elt->getMiddleDotsNumber ();
  int lowDotsNumber =
    elt->getLowDotsNumber ();

  string numbersToBeUsed;
  bool   nonZeroNumberHasBeenIssued = false;

  // the numbers should be written in the order 'high, middle, low'
  // and 0 ahead of the specification is forbidden

  if (highDotsNumber > 0) {
    numbersToBeUsed +=
      to_string (highDotsNumber);
    nonZeroNumberHasBeenIssued = true;
  }

  if (middleDotsNumber > 0) {
    numbersToBeUsed +=
      to_string (middleDotsNumber);
    nonZeroNumberHasBeenIssued = true;
  }
  else {
    if (nonZeroNumberHasBeenIssued) {
      numbersToBeUsed +=
        to_string (middleDotsNumber);
    }
  }

  numbersToBeUsed +=
    to_string (lowDotsNumber);

  fLilypondCodeIOstream <<
    "\\discant \"" <<
    numbersToBeUsed <<
    "\" ";
}

void lpsr2LilypondTranslator::visitStart (S_msrHarpPedalsTuning& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrHarpPedalsTuning" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  map<msrDiatonicPitchKind, msrAlterationKind>
    harpPedalsAlterationKindsMap =
      elt->getHarpPedalsAlterationKindsMap ();

  if (harpPedalsAlterationKindsMap.size ()) {
    gIndenter++;

    fLilypondCodeIOstream <<
      "_\\markup { \\harp-pedal #\"" <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kD]) <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kC]) <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kB]) <<
      "|" <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kE]) <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kF]) <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kG]) <<
      harpPedalTuningAsLilypondString (
        harpPedalsAlterationKindsMap [kA]) <<
      "\" } " <<
      endl;
  }
  else {
    fLilypondCodeIOstream <<
      "%{empty harp pedals tuning???%} "; // JMI
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrStem& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrStem" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrStem& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrStem" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrBeam& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrBeam" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrBeam& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrBeam" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateCodeForOctaveShift (
  S_msrOctaveShift octaveShift)
{
  msrOctaveShift::msrOctaveShiftKind
    octaveShiftKind =
      octaveShift->
        getOctaveShiftKind ();

  int
    octaveShiftSize =
      octaveShift->
        getOctaveShiftSize ();

  switch (octaveShiftKind) {
    case msrOctaveShift::kOctaveShiftNone:
      break;
    case msrOctaveShift::kOctaveShiftUp:
      fLilypondCodeIOstream <<
      "\\ottava #" <<
        "-" << (octaveShiftSize - 1) / 7 << // 1 or 2
        ' ';
      break;
    case msrOctaveShift::kOctaveShiftDown:
      fLilypondCodeIOstream <<
        "\\ottava #" <<
        (octaveShiftSize - 1) / 7 << // 1 or 2
        ' ';
      break;
    case msrOctaveShift::kOctaveShiftStop:
          fLilypondCodeIOstream <<
            "\\ottava #0 ";
      break;
    case msrOctaveShift::kOctaveShiftContinue:
      break;
  } // switch
}

//________________________________________________________________________
void lpsr2LilypondTranslator::generateCodeBeforeChordContents (S_msrChord chord)
{
  // print the chord's grace notes before if any,
  // but not ??? JMI
  S_msrGraceNotesGroup
    chordGraceNotesGroupBefore =
      chord->getChordGraceNotesGroupBefore ();

/* JMI
  gLogIOstream <<
    "% chordGraceNotesGroupBefore = ";
  if (chordGraceNotesGroupBefore) {
    gLogIOstream <<
      chordGraceNotesGroupBefore;
  }
  else {
    gLogIOstream <<
      "nullptr";
  }
  gLogIOstream << endl;
*/

  if (chordGraceNotesGroupBefore) {
    generateGraceNotesGroup (
      chordGraceNotesGroupBefore);
  }

  // get the chord glissandos
  const list<S_msrGlissando>&
    chordGlissandos =
      chord->getChordGlissandos ();

  // print the chord glissandos styles if any
  if (chordGlissandos.size ()) {
    list<S_msrGlissando>::const_iterator i;
    for (
      i=chordGlissandos.begin ();
      i!=chordGlissandos.end ();
      i++
    ) {
      S_msrGlissando glissando = (*i);

      switch (glissando->getGlissandoTypeKind ()) {
        case msrGlissando::kGlissandoTypeNone:
          break;

        case msrGlissando::kGlissandoTypeStart:
          // generate the glissando style
          switch (glissando->getGlissandoLineTypeKind ()) {
            case kLineTypeSolid:
              break;
            case kLineTypeDashed:
              fLilypondCodeIOstream <<
                "\\once\\override Glissando.style = #'dashed-line ";
              break;
            case kLineTypeDotted:
              fLilypondCodeIOstream <<
                "\\once\\override Glissando.style = #'dotted-line ";
              break;
            case kLineTypeWavy:
              fLilypondCodeIOstream <<
                "\\once\\override Glissando.style = #'zigzag ";
              break;
          } // switch
          break;

        case msrGlissando::kGlissandoTypeStop:
          break;
      } // switch
    } // for
  }

  // get the chord slides
  const list<S_msrSlide>&
    chordSlides =
      chord->getChordSlides ();

  // print the chord slides styles if any, implemented as glissandos
  if (chordSlides.size ()) {
    list<S_msrSlide>::const_iterator i;
    for (
      i=chordSlides.begin ();
      i!=chordSlides.end ();
      i++
    ) {
      S_msrSlide slide = (*i);

      switch (slide->getSlideTypeKind ()) {
        case msrSlide::kSlideTypeNone:
          break;

        case msrSlide::kSlideTypeStart:
          // generate the glissando style
          switch (slide->getSlideLineTypeKind ()) {
            case kLineTypeSolid:
              break;
            case kLineTypeDashed:
              fLilypondCodeIOstream <<
                "\\once\\override Glissando.style = #'dashed-line ";
              break;
            case kLineTypeDotted:
              fLilypondCodeIOstream <<
                "\\once\\override Glissando.style = #'dotted-line ";
              break;
            case kLineTypeWavy:
              fLilypondCodeIOstream <<
                "\\once\\override Glissando.style = #'zigzag ";
              break;
          } // switch
          break;

        case msrSlide::kSlideTypeStop:
          break;
      } // switch
    } // for
  }

  // get the chord ligatures
  list<S_msrLigature>
    chordLigatures =
      chord->getChordLigatures ();

  // print the chord ligatures if any
  if (chordLigatures.size ()) {
    list<S_msrLigature>::const_iterator i;
    for (
      i=chordLigatures.begin ();
      i!=chordLigatures.end ();
      i++
    ) {

      switch ((*i)->getLigatureKind ()) {
        case msrLigature::kLigatureNone:
          break;
        case msrLigature::kLigatureStart:
          fLilypondCodeIOstream << "\\[ ";
          break;
        case msrLigature::kLigatureContinue:
          break;
        case msrLigature::kLigatureStop:
  // JMI        fLilypondCodeIOstream << "\\] ";
          break;
      } // switch
    } // for
  }

  // don't take the chord into account for line breaking ??? JMI

  // get the chord articulations
  list<S_msrArticulation>
    chordArticulations =
      chord->getChordArticulations ();

  // print the chord arpeggios directions if any
  if (chordArticulations.size ()) {
    list<S_msrArticulation>::const_iterator i;
    for (
      i=chordArticulations.begin ();
      i!=chordArticulations.end ();
      i++
    ) {
      S_msrArticulation articulation = (*i);

      if (
        // arpeggiato?
        S_msrArpeggiato
          arpeggiato =
            dynamic_cast<msrArpeggiato*>(&(*articulation))
        ) {
        msrDirectionKind
          directionKind =
            arpeggiato->getArpeggiatoDirectionKind ();

        switch (directionKind) {
          case kDirectionNone:
            fLilypondCodeIOstream <<
              endl <<
              "\\arpeggioNormal";
            break;
          case kDirectionUp:
            fLilypondCodeIOstream <<
              endl <<
              "\\arpeggioArrowUp";
            break;
          case kDirectionDown:
            fLilypondCodeIOstream <<
              endl <<
              "\\arpeggioArrowDown";
            break;
        } // switch

        fLilypondCodeIOstream << ' ';

        fCurrentArpeggioDirectionKind = directionKind;
      }

      else if (
        // non arpeggiato?
        S_msrNonArpeggiato
          nonArpeggiato =
            dynamic_cast<msrNonArpeggiato*>(&(*articulation))
        ) {
        fLilypondCodeIOstream <<
          endl <<
          "\\arpeggioBracket";

        switch (nonArpeggiato->getNonArpeggiatoTypeKind ()) {
          case msrNonArpeggiato::kNonArpeggiatoTypeNone:
            fLilypondCodeIOstream << " %{\\kNonArpeggiatoTypeNone???%}";
            break;
          case msrNonArpeggiato::kNonArpeggiatoTypeTop:
            fLilypondCodeIOstream << " %{\\kNonArpeggiatoTypeTop???%}";
            break;
          case msrNonArpeggiato::kNonArpeggiatoTypeBottom:
            fLilypondCodeIOstream << " %{\\kNonArpeggiatoTypeBottom???%}";
            break;
        } // switch

        fLilypondCodeIOstream << endl;
      }
   } // for
  }

  // should stem direction be generated?
  const list<S_msrStem>&
    chordStems =
      chord->getChordStems ();

  if (chordStems.size ()) {
   list<S_msrStem>::const_iterator
      iBegin = chordStems.begin (),
      iEnd   = chordStems.end (),
      i      = iBegin;

    for ( ; ; ) {
      S_msrStem stem = (*i);

      switch (stem->getStemKind ()) {
        case msrStem::kStemNone:
          fLilypondCodeIOstream << "\\stemNeutral ";
          break;
        case msrStem::kStemUp:
          fLilypondCodeIOstream << "\\stemUp ";
          break;
        case msrStem::kStemDown:
          fLilypondCodeIOstream << "\\stemDown ";
          break;
        case msrStem::kStemDouble: // JMI ???
          break;
      } // switch

      if (++i == iEnd) break;
      fLilypondCodeIOstream <<
        ' ';
    } // for

    fLilypondCodeIOstream <<
      ' ';
  }

  // should an octave shift be generated?
  S_msrOctaveShift
    chordOctaveShift =
      chord->getChordOctaveShift ();

  if (chordOctaveShift) {
    generateCodeForOctaveShift (
      chordOctaveShift);
  }

  // generate the start of the chord
  fLilypondCodeIOstream <<
    "<";

  fOnGoingChord = true;
}

void lpsr2LilypondTranslator::generateCodeForChordContents (S_msrChord chord)
{
  // get the chord notes vector
  const vector<S_msrNote>&
    chordNotesVector =
      chord->getChordNotesVector ();

  // generate the chord notes KOF JMI
  if (chordNotesVector.size ()) {
    vector<S_msrNote>::const_iterator
      iBegin = chordNotesVector.begin (),
      iEnd   = chordNotesVector.end (),
      i      = iBegin;
    for ( ; ; ) {
      S_msrNote
        note = (*i);

      // print things before the note
      generateCodeBeforeNote (note);

      // print the note itself
      generateCodeForNote (note);

      // print things after the note
      generateCodeAfterNote (note);

      if (++i == iEnd) break;
      fLilypondCodeIOstream <<
        ' ';
    } // for
  }
}

void lpsr2LilypondTranslator::generateCodeAfterChordContents (S_msrChord chord)
{
  // generate the end of the chord
  fLilypondCodeIOstream <<
    ">";

  // get the chord notes vector
  const vector<S_msrNote>&
    chordNotesVector =
      chord->getChordNotesVector ();

  // if the preceding item is a chord, the first note of the chord
  // is used as the reference point for the octave placement
  // of a following note or chord
  switch (gLilypondOptions->fOctaveEntryKind) {
    case kOctaveEntryRelative:
      fCurrentOctaveEntryReference =
        chordNotesVector [0];
      break;
    case kOctaveEntryAbsolute:
      break;
    case kOctaveEntryFixed:
      break;
  } // switch

  // generate the chord duration if relevant
  if (
    chord->getChordIsFirstChordInADoubleTremolo ()
      ||
    chord->getChordIsSecondChordInADoubleTremolo ()) {
      // print chord duration
      fLilypondCodeIOstream <<
        chord->getChordSoundingWholeNotes ();
  }

  else {
    int chordInputLineNumber =
      chord->getInputLineNumber ();

    // print the chord duration
    fLilypondCodeIOstream <<
      durationAsLilypondString (
        chordInputLineNumber,
        chord->
          getChordDisplayWholeNotes ()); // JMI test wether chord is in a tuplet?
  }

  // are there pending chord member notes string numbers?
  if (fPendingChordMemberNotesStringNumbers.size ()) {
    list<int>::const_iterator
      iBegin = fPendingChordMemberNotesStringNumbers.begin (),
      iEnd   = fPendingChordMemberNotesStringNumbers.end (),
      i      = iBegin;

    for ( ; ; ) {
      int stringNumber = (*i);

      fLilypondCodeIOstream <<
        "\\" <<
        stringNumber;

      if (++i == iEnd) break;
      fLilypondCodeIOstream <<
        ' ';
    } // for
    fLilypondCodeIOstream <<
      ' ';

    // forget about the pending string numbers
    fPendingChordMemberNotesStringNumbers.clear ();
  }

  // fetch the chord single tremolo
  S_msrSingleTremolo
    chordSingleTremolo =
      chord->getChordSingleTremolo ();

  if (chordSingleTremolo) {
    // generate code for the chord single tremolo
    fLilypondCodeIOstream <<
      singleTremoloDurationAsLilypondString (
        chordSingleTremolo);
  }

  fLilypondCodeIOstream <<
    ' ';

  // get the chord articulations
  list<S_msrArticulation>
    chordArticulations =
      chord->getChordArticulations ();

  // print the chord articulations if any
  if (chordArticulations.size ()) {
    list<S_msrArticulation>::const_iterator i;
    for (
      i=chordArticulations.begin ();
      i!=chordArticulations.end ();
      i++
    ) {
      generateChordArticulation ((*i));

      fLilypondCodeIOstream <<
        ' ';
    } // for
  }

  // print the chord technicals if any
  list<S_msrTechnical>
    chordTechnicals =
      chord->getChordTechnicals ();

  if (chordTechnicals.size ()) {
    list<S_msrTechnical>::const_iterator i;
    for (
      i=chordTechnicals.begin ();
      i!=chordTechnicals.end ();
      i++
    ) {
      fLilypondCodeIOstream <<
        technicalAsLilypondString ((*i)) <<
        ' '; // JMI
    } // for
  }

  // print the chord technicals with integer if any
  list<S_msrTechnicalWithInteger>
    chordTechnicalWithIntegers =
      chord->getChordTechnicalWithIntegers ();

  if (chordTechnicalWithIntegers.size ()) {
    list<S_msrTechnicalWithInteger>::const_iterator i;
    for (
      i=chordTechnicalWithIntegers.begin ();
      i!=chordTechnicalWithIntegers.end ();
      i++
    ) {
      fLilypondCodeIOstream <<
        technicalWithIntegerAsLilypondString ((*i)) <<
        ' '; // JMI
    } // for
  }

  // print the chord technicals with float if any
  list<S_msrTechnicalWithFloat>
    chordTechnicalWithFloats =
      chord->getChordTechnicalWithFloats ();

  if (chordTechnicalWithFloats.size ()) {
    list<S_msrTechnicalWithFloat>::const_iterator i;
    for (
      i=chordTechnicalWithFloats.begin ();
      i!=chordTechnicalWithFloats.end ();
      i++
    ) {
      fLilypondCodeIOstream <<
        technicalWithFloatAsLilypondString ((*i)) <<
        ' '; // JMI
    } // for
  }

  // print the chord technicals with string if any
  list<S_msrTechnicalWithString>
    chordTechnicalWithStrings =
      chord->getChordTechnicalWithStrings ();

  if (chordTechnicalWithStrings.size ()) {
    list<S_msrTechnicalWithString>::const_iterator i;
    for (
      i=chordTechnicalWithStrings.begin ();
      i!=chordTechnicalWithStrings.end ();
      i++
    ) {
      fLilypondCodeIOstream <<
        technicalWithStringAsLilypondString ((*i)) <<
        ' '; // JMI
    } // for
  }

  // print the chord ornaments if any
  list<S_msrOrnament>
    chordOrnaments =
      chord->getChordOrnaments ();

  if (chordOrnaments.size ()) {
    list<S_msrOrnament>::const_iterator i;
    for (
      i=chordOrnaments.begin ();
      i!=chordOrnaments.end ();
      i++
    ) {
      S_msrOrnament
        ornament = (*i);

      switch (ornament->getOrnamentPlacementKind ()) {
        case kPlacementNone:
          fLilypondCodeIOstream << "-";
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          fLilypondCodeIOstream << "-";
          break;
      } // switch

      generateOrnament (ornament); // some ornaments are not yet supported
    } // for
  }

  // print the chord dynamics if any
  list<S_msrDynamics>
    chordDynamics =
      chord->getChordDynamics ();

  if (chordDynamics.size ()) {
    list<S_msrDynamics>::const_iterator i;
    for (
      i=chordDynamics.begin ();
      i!=chordDynamics.end ();
      i++
    ) {
      S_msrDynamics
        dynamics = (*i);

      switch (dynamics->getDynamicsPlacementKind ()) {
        case kPlacementNone:
          fLilypondCodeIOstream << "-";
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          // this is done by LilyPond by default
          break;
      } // switch

      fLilypondCodeIOstream <<
        dynamicsAsLilypondString (dynamics) << ' ';
    } // for
  }

  // print the chord other dynamics if any
  list<S_msrOtherDynamics>
    chordOtherDynamics =
      chord->getChordOtherDynamics ();

  if (chordOtherDynamics.size ()) {
    list<S_msrOtherDynamics>::const_iterator i;
    for (
      i=chordOtherDynamics.begin ();
      i!=chordOtherDynamics.end ();
      i++
    ) {
      S_msrOtherDynamics
        otherDynamics = (*i);

      switch (otherDynamics->getOtherDynamicsPlacementKind ()) {
        case kPlacementNone:
          fLilypondCodeIOstream << "-";
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          // this is done by LilyPond by default
          break;
      } // switch

      fLilypondCodeIOstream <<
        "\\" << otherDynamics->asString () << ' ';
    } // for
  }

  // print the chord words if any
  list<S_msrWords>
    chordWords =
      chord->getChordWords ();

  if (chordWords.size ()) {
    list<S_msrWords>::const_iterator i;
    for (
      i=chordWords.begin ();
      i!=chordWords.end ();
      i++
    ) {

      msrPlacementKind
        wordsPlacementKind =
          (*i)->getWordsPlacementKind ();

      string wordsContents =
        (*i)->getWordsContents ();

      switch (wordsPlacementKind) {
        case kPlacementNone:
          // should not occur
          break;
        case kPlacementAbove:
          fLilypondCodeIOstream << "^";
          break;
        case kPlacementBelow:
          fLilypondCodeIOstream << "_";
          break;
      } // switch

      fLilypondCodeIOstream <<
        "\\markup" << " { " <<
        quoteStringIfNonAlpha (wordsContents) <<
        " } ";
    } // for
  }

  // print the chord beams if any
  list<S_msrBeam>
    chordBeams =
      chord->getChordBeams ();

  if (chordBeams.size ()) {
    list<S_msrBeam>::const_iterator i;
    for (
      i=chordBeams.begin ();
      i!=chordBeams.end ();
      i++
    ) {

      S_msrBeam beam = (*i);

      // LilyPond will take care of multiple beams automatically,
      // so we need only generate code for the first number (level)
      switch (beam->getBeamKind ()) {

        case msrBeam::kBeginBeam:
          if (beam->getBeamNumber () == 1)
            fLilypondCodeIOstream << "[ ";
          break;

        case msrBeam::kContinueBeam:
          break;

        case msrBeam::kEndBeam:
          if (beam->getBeamNumber () == 1)
            fLilypondCodeIOstream << "] ";
          break;

        case msrBeam::kForwardHookBeam:
          break;

        case msrBeam::kBackwardHookBeam:
          break;

        case msrBeam::k_NoBeam:
          break;
      } // switch
    } // for
  }

  // print the chord slurs if any
  list<S_msrSlur>
    chordSlurs =
      chord->getChordSlurs ();

  if (chordSlurs.size ()) {
    list<S_msrSlur>::const_iterator i;
    for (
      i=chordSlurs.begin ();
      i!=chordSlurs.end ();
      i++
    ) {

      switch ((*i)->getSlurTypeKind ()) {
        case msrSlur::k_NoSlur:
          break;
        case msrSlur::kRegularSlurStart:
          fLilypondCodeIOstream << "( ";
          break;
        case msrSlur::kPhrasingSlurStart:
          fLilypondCodeIOstream << "\\( ";
          break;
        case msrSlur::kSlurContinue:
          break;
        case msrSlur::kRegularSlurStop:
          fLilypondCodeIOstream << ") ";
          break;
        case msrSlur::kPhrasingSlurStop:
          fLilypondCodeIOstream << "\\) ";
          break;
      } // switch
    } // for
  }

/* Don't print the chord ties, rely only on its notes's ties // JMI
  // thus using LilyPond's partially tied chords // JMI
  // print the chord ties if any
  list<S_msrTie>
    chordTies =
      chord->getChordTies ();

  if (chordTies.size ()) {
    list<S_msrTie>::const_iterator i;
    for (
      i=chordTies.begin ();
      i!=chordTies.end ();
      i++
    ) {
      fLilypondCodeIOstream <<
        "%{line: " << inputLineNumber << "%}" <<
        "~ %{S_msrChord}"; // JMI spaces???
    } // for
  }
*/

/* JMI
  // print the tie if any
  {
    S_msrTie chordTie = chord->getChordTie ();

    if (chordTie) {
      if (chordTie->getTieKind () == msrTie::kTieStart) {
        fLilypondCodeIOstream <<
          "%{line: " << inputLineNumber << "%}" <<
          "~ ";
      }
    }
  }
*/

  // get the chord ligatures
  list<S_msrLigature>
    chordLigatures =
      chord->getChordLigatures ();

  // print the chord ligatures if any
  if (chordLigatures.size ()) {
    list<S_msrLigature>::const_iterator i;
    for (
      i=chordLigatures.begin ();
      i!=chordLigatures.end ();
      i++
    ) {
      switch ((*i)->getLigatureKind ()) {
        case msrLigature::kLigatureNone:
          break;
        case msrLigature::kLigatureStart:
          fLilypondCodeIOstream << "\\[ ";
          break;
        case msrLigature::kLigatureContinue:
          break;
        case msrLigature::kLigatureStop:
          fLilypondCodeIOstream << "\\] ";
          break;
      } // switch
    } // for
  }

  // print the chord wedges if any
  list<S_msrWedge>
    chordWedges =
      chord->getChordWedges ();

  if (chordWedges.size ()) {
    list<S_msrWedge>::const_iterator i;
    for (
      i=chordWedges.begin ();
      i!=chordWedges.end ();
      i++
      ) {
      switch ((*i)->getWedgeKind ()) {
        case msrWedge::kWedgeKindNone:
          break;
        case msrWedge::kWedgeCrescendo:
          fLilypondCodeIOstream << "\\< ";
          break;
        case msrWedge::kWedgeDecrescendo:
          fLilypondCodeIOstream << "\\> ";
          break;
        case msrWedge::kWedgeStop:
          fLilypondCodeIOstream << "\\! ";
          break;
      } // switch
    } // for
  }

  // get the chord glissandos
  const list<S_msrGlissando>&
    chordGlissandos =
      chord->getChordGlissandos ();

  // print the chord glissandos if any
  if (chordGlissandos.size ()) {
    list<S_msrGlissando>::const_iterator i;
    for (
      i=chordGlissandos.begin ();
      i!=chordGlissandos.end ();
      i++
    ) {
      S_msrGlissando glissando = (*i);

      switch (glissando->getGlissandoTypeKind ()) {
        case msrGlissando::kGlissandoTypeNone:
          break;

        case msrGlissando::kGlissandoTypeStart:
          // generate the glissando itself
          fLilypondCodeIOstream <<
            "\\glissando ";
          break;

        case msrGlissando::kGlissandoTypeStop:
          break;
      } // switch
    } // for
  }

  // get the chord slides
  const list<S_msrSlide>&
    chordSlides =
      chord->getChordSlides ();

  // print the chord slides if any, implemented as glissandos
  if (chordSlides.size ()) {
    list<S_msrSlide>::const_iterator i;
    for (
      i=chordSlides.begin ();
      i!=chordSlides.end ();
      i++
    ) {
      S_msrSlide slide = (*i);

      switch (slide->getSlideTypeKind ()) {
        case msrSlide::kSlideTypeNone:
          break;

        case msrSlide::kSlideTypeStart:
          // generate the glissando itself
          fLilypondCodeIOstream <<
            "\\glissando ";
          break;

        case msrSlide::kSlideTypeStop:
          break;
      } // switch
    } // for
  }

  fOnGoingChord = false;
}

void lpsr2LilypondTranslator::generateChordInGraceNotesGroup (S_msrChord chord)
{
  generateCodeBeforeChordContents (chord);

  generateCodeForChordContents (chord);

  generateCodeAfterChordContents (chord);
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrChord& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrChord" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

#ifdef TRACE_OPTIONS
  if (fOnGoingGraceNotesGroup) {
    int chordInputLineNumber =
      elt->getInputLineNumber ();

    msrInternalWarning (
      gGeneralOptions->fInputSourceName,
      chordInputLineNumber,
      "% ==> Start visiting grace chords is ignored");

    return;
  }
#endif

  generateCodeBeforeChordContents (elt);
}

void lpsr2LilypondTranslator::visitEnd (S_msrChord& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrChord" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

#ifdef TRACE_OPTIONS
  if (fOnGoingGraceNotesGroup) {
    msrInternalWarning (
      gGeneralOptions->fInputSourceName,
      inputLineNumber,
      "% ==> End visiting grace chords is ignored");

    return;
  }
#endif

  generateCodeAfterChordContents (elt);

  // if the preceding item is a chord, the first note of the chord
  // is used as the reference point for the octave placement
  // of a following note or chord
  switch (gLilypondOptions->fOctaveEntryKind) {
    case kOctaveEntryRelative:
      fCurrentOctaveEntryReference =
        elt->getChordNotesVector () [0];
      break;
    case kOctaveEntryAbsolute:
      break;
    case kOctaveEntryFixed:
      break;
  } // switch

  fOnGoingChord = false;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTuplet& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTuplet" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  if (fTupletsStack.size ()) {
    // elt is a nested tuplet

    S_msrTuplet
      containingTuplet =
        fTupletsStack.top ();

    // unapply containing tuplet factor,
    // i.e 3/2 inside 5/4 becomes 15/8 in MusicXML...
    elt->
      unapplySoundingFactorToTupletMembers (
        containingTuplet->
          getTupletFactor ());
  }

  if (gLilypondOptions->fIndentTuplets) {
    fLilypondCodeIOstream << endl;
  }

  // get bracket kind
  msrTuplet::msrTupletBracketKind
    tupletBracketKind =
      elt->getTupletBracketKind ();

  switch (tupletBracketKind) {
    case msrTuplet::kTupletBracketYes:
    /* JMI
      fLilypondCodeIOstream <<
        "%{kTupletBracketYes%}" <<
        endl;
        */
      break;
    case msrTuplet::kTupletBracketNo:
      fLilypondCodeIOstream <<
        "\\once \\omit TupletBracket" <<
        endl;
      break;
  } // switch

  // get line shape kind
  msrTuplet::msrTupletLineShapeKind
    tupletLineShapeKind =
      elt->getTupletLineShapeKind ();

  switch (tupletLineShapeKind) {
    case msrTuplet::kTupletLineShapeStraight:
      break;
    case msrTuplet::kTupletLineShapeCurved:
      fLilypondCodeIOstream <<
        "\\temporary \\tupletsCurvedBrackets" <<
        endl;
      break;
  } // switch

  // get show number kind
  msrTuplet::msrTupletShowNumberKind
    tupletShowNumberKind =
      elt->getTupletShowNumberKind ();

  switch (tupletShowNumberKind) {
    case msrTuplet::kTupletShowNumberActual:
    /* JMI
      fLilypondCodeIOstream <<
        "%{tupletShowNumberActual%}" <<
        endl;
        */
      break;
    case msrTuplet::kTupletShowNumberBoth:
      fLilypondCodeIOstream <<
        "\\once \\override TupletNumber.text = #tuplet-number::calc-fraction-text" <<
        endl;
      break;
    case msrTuplet::kTupletShowNumberNone:
      fLilypondCodeIOstream <<
        "\\once \\omit TupletNumber" <<
        endl;
      break;
  } // switch

  // get show type kind
  msrTuplet::msrTupletShowTypeKind
    tupletShowTypeKind =
      elt->getTupletShowTypeKind ();

  rational
    memberNoteDisplayWholeNotes =
      elt->getMemberNotesDisplayWholeNotes ();

  switch (tupletShowTypeKind) {
    case msrTuplet::kTupletShowTypeActual:
      fLilypondCodeIOstream <<
     // JMI ???   "\\once \\override TupletNumber.text = #(tuplet-number::append-note-wrapper tuplet-number::calc-fraction-text \"" <<
        "\\once \\override TupletNumber.text = #(tuplet-number::append-note-wrapper tuplet-number::calc-denominator-text \"" <<
        wholeNotesAsLilypondString (
          inputLineNumber,
          memberNoteDisplayWholeNotes) <<
        "\")" <<
        endl;
      break;
    case msrTuplet::kTupletShowTypeBoth:
      fLilypondCodeIOstream <<
        "\\once \\override TupletNumber.text = #(tuplet-number::fraction-with-notes" <<
        " #{ " <<
        wholeNotesAsLilypondString (
          inputLineNumber,
          memberNoteDisplayWholeNotes) <<
        " #} #{ " <<
        wholeNotesAsLilypondString (
          inputLineNumber,
          memberNoteDisplayWholeNotes) <<
        " #})" <<
        endl;
      break;
    case msrTuplet::kTupletShowTypeNone:
    /* JMI
      fLilypondCodeIOstream <<
        "%{tupletShowTypeNone%}" <<
        endl;
        */
      break;
  } // switch

  fLilypondCodeIOstream <<
    "\\tuplet " <<
    elt->getTupletFactor ().asRational () <<
    " {" <<
    endl;

  fTupletsStack.push (elt);

  gIndenter++;

  // force durations to be displayed explicitly
  // at the beginning of the tuplet
  fLastMetWholeNotes = rational (0, 1);
}

void lpsr2LilypondTranslator::visitEnd (S_msrTuplet& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTuplet" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  if (gLilypondOptions->fIndentTuplets) {
    fLilypondCodeIOstream << endl;
  }

  fLilypondCodeIOstream <<
    "}" <<
    endl;

  // get line shape kind
  msrTuplet::msrTupletLineShapeKind
    tupletLineShapeKind =
      elt->getTupletLineShapeKind ();

  switch (tupletLineShapeKind) {
    case msrTuplet::kTupletLineShapeStraight:
      break;
    case msrTuplet::kTupletLineShapeCurved:
      fLilypondCodeIOstream <<
        "\\undo \\tupletsCurvedBrackets" <<
        endl;
      break;
  } // switch

  fTupletsStack.pop ();

/* JMI
 ?????? fCurrentOctaveEntryReference = nullptr;
  // the first note after the tuplet will become the new reference
  */
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrTie& elt)
{
int inputLineNumber =
  elt->getInputLineNumber ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrTie" <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  switch (elt->getTieKind ()) {
    case msrTie::kTieNone:
      break;
    case msrTie::kTieStart:
      if (fOnGoingNote) {
        // this precludes generating for the chords' ties,
        // since the last of its notes sets fOnGoingNote to false
        // after code has been generated for it
        fLilypondCodeIOstream <<
  // JMI        "%{line: " << inputLineNumber << "%}" <<
          " ~ ";
      }
      break;
    case msrTie::kTieContinue:
      break;
    case msrTie::kTieStop:
      break;
   } // switch
}

void lpsr2LilypondTranslator::visitEnd (S_msrTie& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrTie" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrSegno& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrSegno" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\mark \\markup { \\musicglyph #\"scripts.segno\" }" <<
    endl;
}

void lpsr2LilypondTranslator::visitStart (S_msrCoda& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrCoda" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\mark \\markup { \\musicglyph #\"scripts.coda\" }" <<
    endl;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrEyeGlasses& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting eyeGlasses" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "^\\markup {\\eyeglasses} ";
}

void lpsr2LilypondTranslator::visitStart (S_msrScordatura& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting scordatura" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

/* JMI
  const list<S_msrStringTuning>&
    scordaturaStringTuningsList =
      elt->getScordaturaStringTuningsList ();

  gIndenter++;

  fLilypondCodeIOstream <<
    "\\new Staff {" <<
    endl;

  gIndenter++;

  fLilypondCodeIOstream <<
    "\\hide Staff.Stem" <<
    endl <<
    "\\hide Staff.TimeSignature" <<
    endl <<
    "\\small" <<
    endl <<
    "<";

  if (scordaturaStringTuningsList.size ()) {
    list<S_msrStringTuning>::const_iterator
      iBegin = scordaturaStringTuningsList.begin (),
      iEnd   = scordaturaStringTuningsList.end (),
      i      = iBegin;
    for ( ; ; ) {
      S_msrStringTuning
        stringTuning = (*i);

      fLilypondCodeIOstream <<
        stringTuningAsLilypondString (
          elt->getInputLineNumber (),
          stringTuning);

      if (++i == iEnd) break;

      fLilypondCodeIOstream << ' ';
    } // for
  }

  fLilypondCodeIOstream <<
    ">4" <<
    endl <<
    "}" <<
    endl;

  gIndenter--;

  fLilypondCodeIOstream <<
    "{ c'4 }" <<
    endl <<

  gIndenter--;
  */
}

void lpsr2LilypondTranslator::visitStart (S_msrPedal& elt)
{
  int inputLineNumber =
    elt->getInputLineNumber ();

#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting pedal" <<
      ", line " << inputLineNumber <<
      endl;
  }
#endif

  fLilypondCodeIOstream << endl;

  switch (elt->getPedalTypeKind ()) {
    case msrPedal::kPedalStart:
      fLilypondCodeIOstream <<
        "\\sustainOn";
      break;
    case msrPedal::kPedalContinue:
      fLilypondCodeIOstream <<
        "\\sustainOff\\sustainOn"; // JMI
      break;
    case msrPedal::kPedalChange:
      fLilypondCodeIOstream <<
        "\\sustainOff\\sustainOn";
      break;
    case msrPedal::kPedalStop:
      fLilypondCodeIOstream <<
        "\\sustainOff";
      break;
    case msrPedal::k_NoPedalType:
      {
        // should not occur

        stringstream s;

        s <<
          "msrPedal '" <<
          elt->asShortString () <<
          "' has no pedal type";

        msrInternalError (
          gGeneralOptions->fInputSourceName,
          inputLineNumber,
          __FILE__, __LINE__,
          s.str ());
      }
      break;
  } // switch

  fLilypondCodeIOstream << endl;
}

void lpsr2LilypondTranslator::visitStart (S_msrDamp& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting damp" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "^\\markup {\\damp} ";
}

void lpsr2LilypondTranslator::visitStart (S_msrDampAll& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting dampAll" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "^\\markup {\\dampAll} ";
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrBarline& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      endl <<
      "% --> Start visiting msrBarline" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  switch (elt->getBarlineCategory ()) {

    case msrBarline::kBarlineCategoryStandalone:
      switch (elt->getBarlineStyleKind ()) {
        case msrBarline::kBarlineStyleNone:
          break;
        case msrBarline::kBarlineStyleRegular:
          fLilypondCodeIOstream << "\\bar \"|\" ";
          break;
        case msrBarline::kBarlineStyleDotted:
          fLilypondCodeIOstream << "\\bar \";\" ";
          break;
        case msrBarline::kBarlineStyleDashed:
          fLilypondCodeIOstream << "\\bar \"!\" ";
          break;
        case msrBarline::kBarlineStyleHeavy:
          fLilypondCodeIOstream << "\\bar \".\" ";
          break;
        case msrBarline::kBarlineStyleLightLight:
          fLilypondCodeIOstream << "\\bar \"||\" ";
          break;
        case msrBarline::kBarlineStyleLightHeavy:
          fLilypondCodeIOstream <<
            endl <<
            "\\bar \"|.\" ";
          break;
        case msrBarline::kBarlineStyleHeavyLight:
          fLilypondCodeIOstream << "\\bar \".|\" ";
          break;
        case msrBarline::kBarlineStyleHeavyHeavy:
          fLilypondCodeIOstream << "\\bar \"..\" ";
          break;
        case msrBarline::kBarlineStyleTick:
          fLilypondCodeIOstream << "\\bar \"'\" ";
          break;
        case msrBarline::kBarlineStyleShort:
          // \bar "/" is the custom short barline
          fLilypondCodeIOstream << "\\bar \"/\" ";
          break;
          /* JMI
        case msrBarline::kBarlineStyleNone:
          fLilypondCodeIOstream << "\\bar \"\" ";
          break;
          */
      } // switch

      if (gLilypondOptions->fInputLineNumbers) {
        // print the barline line number as a comment
        fLilypondCodeIOstream <<
          "%{ " << inputLineNumber << " %} ";
      }

      fLilypondCodeIOstream << endl;

      switch (elt->getBarlineHasSegnoKind ()) {
        case msrBarline::kBarlineHasSegnoYes:
          fLilypondCodeIOstream <<
            "\\mark \\markup { \\musicglyph #\"scripts.segno\" } ";
          break;
        case msrBarline::kBarlineHasSegnoNo:
          break;
      } // switch

      switch (elt->getBarlineHasCodaKind ()) {
        case msrBarline::kBarlineHasCodaYes:
          fLilypondCodeIOstream <<
            "\\mark \\markup { \\musicglyph #\"scripts.coda\" } ";
          break;
        case msrBarline::kBarlineHasCodaNo:
          break;
      } // switch
      break;

    case msrBarline::kBarlineCategoryRepeatStart:
      if (gLilypondOptions->fKeepRepeatBarlines) {
        fLilypondCodeIOstream << "\\bar \".|:\" ";
      }
      break;
    case msrBarline::kBarlineCategoryRepeatEnd:
      if (gLilypondOptions->fKeepRepeatBarlines) {
        fLilypondCodeIOstream << "\\bar \":|.|:\" ";
      }
      break;

    case msrBarline::kBarlineCategoryHookedEndingStart:
    case msrBarline::kBarlineCategoryHookedEndingEnd:
    case msrBarline::kBarlineCategoryHooklessEndingStart:
    case msrBarline::kBarlineCategoryHooklessEndingEnd:
      // should not occur, since
      // LilyPond will take care of displaying the repeat
      break;

    case msrBarline::k_NoBarlineCategory:
      {
        stringstream s;

        s <<
          "barline category has not been set" <<
          ", line " << elt->getInputLineNumber ();

  // JMI      msrInternalError (
        msrInternalWarning (
          gGeneralOptions->fInputSourceName,
          inputLineNumber,
  // JMI        __FILE__, __LINE__,
          s.str ());
      }
      break;
  } // switch

  if (gLilypondOptions->fInputLineNumbers) {
    fLilypondCodeIOstream <<
      " %{ " << inputLineNumber << " %}";
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrBarline& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      endl <<
      "% --> End visiting msrBarline" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrBarCheck& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrBarCheck" <<
      ", nextBarNumber: " <<
      elt->getNextBarPuristNumber () <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int nextBarPuristNumber =
    elt->getNextBarPuristNumber ();

  // don't generate a bar check before the end of measure 1 // JMI ???
  fLilypondCodeIOstream <<
    "| % " << nextBarPuristNumber <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_msrBarCheck& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrBarCheck" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrBarNumberCheck& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrBarNumberCheck" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (! fOnGoingVoiceCadenza) { // should be tested in msr2LpsrTranslator.cpp JMI visitEnd (S_msrMeasure&)
    // MusicXML bar numbers cannot be relied upon for a LilyPond bar number check
    int nextBarPuristNumber =
      elt->getNextBarPuristNumber ();

    fLilypondCodeIOstream <<
      "\\barNumberCheck #" <<
      nextBarPuristNumber <<
      endl;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrBarNumberCheck& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrBarNumberCheck" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrLineBreak& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrLineBreak" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\myBreak | % " << elt->getNextBarNumber () <<
    endl <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_msrLineBreak& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrLineBreak" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrPageBreak& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrPageBreak" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "\\myPageBreak " <<
    endl <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_msrPageBreak& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrPageBreak" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrRepeat& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrRepeat" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int repeatEndingsNumber =
    elt->getRepeatEndings ().size ();

  if (repeatEndingsNumber == 0)
    repeatEndingsNumber = 2; // implicitly JMI ???

  fRepeatDescrsStack.push_back (
    lpsrRepeatDescr::create (
      repeatEndingsNumber));

  int
    repeatTimes =
      elt->getRepeatTimes ();

  stringstream s;

  fLilypondCodeIOstream << endl;

  s <<
    "\\repeat volta " <<
    repeatTimes <<
// JMI    fRepeatDescrsStack.back ()->getRepeatEndingsNumber () <<
    " {";

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      s.str () << "% start of repeat";
  }
  else {
    fLilypondCodeIOstream <<
      s.str ();
  }

  fLilypondCodeIOstream << endl;

  gIndenter++;

  if (repeatTimes > 2) {
    fLilypondCodeIOstream <<
      "<>^\"" << repeatTimes << " times\"" << // JMI
      endl;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrRepeat& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrRepeat" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  /*
    CAUTION:
      the end of the repeat has to be generated
      before the endings are handled
  */

  if (! fRepeatDescrsStack.back ()->getEndOfRepeatHasBeenGenerated ()) {
    // the end of the repeat has not been generated yet

    gIndenter--;

    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        "}" << "% end of repeat" <<
        endl;
    }
    else {
      fLilypondCodeIOstream <<
        endl <<
        "}" <<
        endl <<
        endl;
    }
  }

  fRepeatDescrsStack.pop_back ();
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrRepeatCommonPart& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrRepeatCommonPart" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

void lpsr2LilypondTranslator::visitEnd (S_msrRepeatCommonPart& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrRepeatCommonPart" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrRepeatEnding& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrRepeatEnding" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fRepeatDescrsStack.back ()->
    incrementRepeatEndingsCounter ();

  int
    repeatEndingInternalNumber =
      elt->getRepeatEndingInternalNumber ();

  if (repeatEndingInternalNumber == 1) {

    gIndenter--;

    // first repeat ending is in charge of
    // outputting the end of the repeat
    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream <<
        setw (commentFieldWidth) << left <<
        "}" << "% end of repeat" <<
        endl;
    }
    else {
      fLilypondCodeIOstream <<
        endl <<
        "}" <<
        endl;
    }
    fRepeatDescrsStack.back ()->
      setEndOfRepeatHasBeenGenerated ();

    // first repeat ending is in charge of
    // outputting the start of the alternative
    if (gLilypondOptions->fComments) {
      fLilypondCodeIOstream << left <<
        endl <<
        setw (commentFieldWidth) <<
        "\\alternative {" <<
        "% start of alternative" <<
        endl;
    }
    else {
      fLilypondCodeIOstream <<
        endl <<
        "\\alternative {" <<
        endl;
    }

    gIndenter++;
  }

  // output the start of the ending
  switch (elt->getRepeatEndingKind ()) {
    case msrRepeatEnding::kHookedEnding:
      if (gLilypondOptions->fComments) {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth) <<
          "{" << "% start of repeat hooked ending" <<
          endl;
      }
      else {
        fLilypondCodeIOstream <<
          "{" <<
          endl;
      }
      break;

    case msrRepeatEnding::kHooklessEnding:
      if (gLilypondOptions->fComments) {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth) <<
          "{" << "% start of repeat hookless ending" <<
          endl;
      }
      else {
        fLilypondCodeIOstream <<
          "{" <<
          endl;
      }
      break;
  } // switch

  gIndenter++;

  // generate the repeat ending number if any
  string
    repeatEndingNumber =
      elt->getRepeatEndingNumber ();

  if (repeatEndingNumber.size ()) {
    if (repeatEndingInternalNumber == 1) {
      fLilypondCodeIOstream <<
        "\\set Score.repeatCommands = #'((volta \"" <<
        repeatEndingNumber <<
        "\"))" <<
        endl;
    }
    else {
      fLilypondCodeIOstream <<
        "\\set Score.repeatCommands = #'(end-repeat (volta \"" <<
        repeatEndingNumber <<
        "\"))" <<
        endl;
    }
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrRepeatEnding& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrRepeatEnding" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  // output the end of the ending
  fLilypondCodeIOstream << endl;

  switch (elt->getRepeatEndingKind ()) {
    case msrRepeatEnding::kHookedEnding:
      if (gLilypondOptions->fComments) {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth) <<
          "}" << "% end of repeat hooked ending" <<
          endl;
      }
      else {
        fLilypondCodeIOstream <<
          "}" <<
          endl;
      }
      break;

    case msrRepeatEnding::kHooklessEnding:
      if (gLilypondOptions->fComments)   {
        fLilypondCodeIOstream << left <<
          setw (commentFieldWidth) <<
          "}" << "% end of repeat hookless ending" <<
          endl;
      }
      else {
        fLilypondCodeIOstream <<
          "}" <<
          endl;
      }
      break;
  } // switch

/* JMI
#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceRepeats) {
    fLilypondCodeIOstream <<
      "% ===**** fRepeatDescrsStack.back () = '" <<
      fRepeatDescrsStack.back ()->repeatDescrAsString () <<
      "'" <<
      endl;
  }
#endif
*/

  if (
    fRepeatDescrsStack.back ()->getRepeatEndingsCounter ()
      ==
    fRepeatDescrsStack.back ()->getRepeatEndingsNumber ()) {

    gIndenter--;

    // last repeat ending is in charge of
    // outputting the end of the alternative
    if (gLilypondOptions->fComments)   {
      fLilypondCodeIOstream << left <<
        setw (commentFieldWidth) <<
        "}" << "% end of alternative" <<
        endl;
    }
    else {
      fLilypondCodeIOstream <<
        "}" <<
        endl <<
        endl;
    }
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrComment& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrComment" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    "% " << elt->getContents () <<
    endl;

  if (elt->getCommentGapKind () == lpsrComment::kGapAfterwards)
    fLilypondCodeIOstream << endl;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrComment& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrComment" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_lpsrSchemeFunction& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting lpsrSchemeFunction" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream <<
    endl <<
    "% Scheme function(s): \"" << elt->getFunctionName () << "\"" <<
    // endl is in the decription
    elt->getFunctionDescription () <<
    // endl is in the decription
    elt->getFunctionCode () <<
    endl <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_lpsrSchemeFunction& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting lpsrSchemeFunction" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrRehearsal& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrRehearsal" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  fLilypondCodeIOstream << endl;

  switch (elt->getRehearsalPlacementKind ()) {
    case msrPlacementKind::kPlacementNone:
      break;
    case msrPlacementKind::kPlacementAbove:
      break;
    case msrPlacementKind::kPlacementBelow:
      fLilypondCodeIOstream <<
        "\\once\\override Score.RehearsalMark.direction = #DOWN";
      break;
    } // switch

  fLilypondCodeIOstream <<
    endl <<
    "\\mark\\markup { ";

  switch (elt->getRehearsalKind ()) {
    case msrRehearsal::kNone:
      fLilypondCodeIOstream <<
        "\\box"; // default value
      break;
    case msrRehearsal::kRectangle:
      fLilypondCodeIOstream <<
        "\\box";
      break;
    case msrRehearsal::kOval:
      fLilypondCodeIOstream <<
        "\\oval";
      break;
    case msrRehearsal::kCircle:
      fLilypondCodeIOstream <<
        "\\circle";
      break;
    case msrRehearsal::kBracket:
      fLilypondCodeIOstream <<
        "\\bracket";
      break;
    case msrRehearsal::kTriangle:
      fLilypondCodeIOstream <<
        "%{\\triangle???%}";
      break;
    case msrRehearsal::kDiamond:
      fLilypondCodeIOstream <<
        "%{\\diamond???";
      break;
  } // switch

  fLilypondCodeIOstream <<
    " { \""<<
    elt->getRehearsalText () <<
    "\"" "}}" <<
    endl;
}

void lpsr2LilypondTranslator::visitEnd (S_msrRehearsal& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrRehearsal" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrMeasuresRepeat& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrMeasuresRepeat" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

#ifdef TRACE_OPTIONS
  int replicasMeasuresNumber =
    elt->measuresRepeatReplicasMeasuresNumber ();
#endif

  int replicasNumber =
    elt->measuresRepeatReplicasNumber ();

#ifdef TRACE_OPTIONS
  if (gTraceOptions->fTraceMeasuresRepeats) {
    int repeatMeasuresNumber =
      elt->measuresRepeatPatternMeasuresNumber ();

    fLilypondCodeIOstream <<
      "% measures repeat, line " << elt->getInputLineNumber () << ":" <<
      endl;

    const int fieldWidth = 24;

    fLilypondCodeIOstream << left <<
      setw (fieldWidth) <<
      "% repeatMeasuresNumber" << " = " << repeatMeasuresNumber <<
      endl <<
      setw (fieldWidth) <<
      "% replicasMeasuresNumber" << " = " << replicasMeasuresNumber <<
      endl <<
      setw (fieldWidth) <<
      "% replicasNumber" << " = " << replicasNumber <<
      endl;
  }
#endif

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% start of measures repeat" <<
      singularOrPlural (
        elt->measuresRepeatReplicasNumber (),
        "replica",
        "replicas") <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }

  fLilypondCodeIOstream <<
    endl <<
    endl <<
    "\\repeat percent " <<
    replicasNumber + 1 <<
     " { " <<
    endl;

    gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_msrMeasuresRepeat& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrMeasuresRepeat" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  gIndenter--;

  fLilypondCodeIOstream <<
    endl <<
    endl <<
    " }" <<
    endl;

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream <<
      setw (commentFieldWidth) << left <<
      "% end of measures repeat" <<
      singularOrPlural (
        elt->measuresRepeatReplicasNumber (),
        "replica",
        "replicas") <<
      ", line " << elt->getInputLineNumber () <<
      endl <<
      endl;
  }
}

void lpsr2LilypondTranslator::visitStart (S_msrMeasuresRepeatPattern& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> Start visiting msrMeasuresRepeatPattern" <<
      endl;
  }
#endif

  gIndenter++;

  // JMI
}

void lpsr2LilypondTranslator::visitEnd (S_msrMeasuresRepeatPattern& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> End visiting msrMeasuresRepeatPattern" <<
      endl;
  }
#endif

  gIndenter--;
}

void lpsr2LilypondTranslator::visitStart (S_msrMeasuresRepeatReplicas& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> Start visiting msrMeasuresRepeatReplicas" <<
      endl;
  }
#endif

  // output the start of the ending
  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "{" << "% start of measures repeat replicas" <<
      endl;
  }
  else {
    fLilypondCodeIOstream <<
      endl <<
      "{" <<
      endl;
  }

  gIndenter++;
}

void lpsr2LilypondTranslator::visitEnd (S_msrMeasuresRepeatReplicas& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> End visiting msrMeasuresRepeatReplicas" <<
      endl;
  }
#endif

  gIndenter--;
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrRestMeasures& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrRestMeasures" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  int restMeasuresNumber =
    elt->getRestMeasuresNumber ();

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% start of rest measures" <<
      singularOrPlural (
        restMeasuresNumber,
        "measure",
        "measures") <<
      ", line " << inputLineNumber <<
      endl <<
      endl;

    gIndenter++;
  }

  fOnGoingRestMeasures = true;
}

void lpsr2LilypondTranslator::visitEnd (S_msrRestMeasures& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrRestMeasures" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  int restMeasuresNumber =
    elt->getRestMeasuresNumber ();

  // start counting measures
  fRemainingRestMeasuresNumber =
    elt->getRestMeasuresNumber ();

  // get rest measures sounding notes
  rational
    restMeasuresMeasureSoundingNotes =
      elt->getRestMeasuresMeasureSoundingNotes ();

  // generate rest measures compression if relevant
  // right befoe Ri*n, because if affects only the next music element
  if (
    fCurrentVoice->getVoiceContainsRestMeasures ()
      ||
    gLilypondOptions->fCompressRestMeasures
  ) {
    fLilypondCodeIOstream <<
      "\\compressMMRests" <<
      endl;
  }

  // generate rest measures only now, in case there are
  // clef, keys or times before them in the first measure
  fLilypondCodeIOstream <<
    "R" <<
    restMeasuresWholeNoteAsLilypondString (
      inputLineNumber,
      restMeasuresMeasureSoundingNotes);

  if (restMeasuresNumber > 1) {
    fLilypondCodeIOstream <<
      "*" <<
      restMeasuresNumber;
  }

  if (gLilypondOptions->fInputLineNumbers) {
    // print the rest measures line number as a comment
    fLilypondCodeIOstream <<
      " %{ " << inputLineNumber << " %} ";
  }

  // wait until all measures have be visited
  // before the bar check is generated // JMI ???

  // now we can generate the bar check
  fLilypondCodeIOstream <<
    " | % " <<
    elt->getRestMeasuresLastMeasurePuristMeasureNumber () + 1 <<
    endl;

  if (gLilypondOptions->fComments) {
    gIndenter--;

    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% end of rest measures" <<
      singularOrPlural (
        restMeasuresNumber,
        "measure",
        "measures") <<
      ", line " << inputLineNumber <<
      endl <<
      endl;
  }

  fOnGoingRestMeasures = false;
}

void lpsr2LilypondTranslator::visitStart (S_msrRestMeasuresContents& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> Start visiting msrRestMeasuresContents" <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% start of rest measures contents " <<
      /* JMI
      singularOrPlural (
        restMeasuresNumber,
        "measure",
        "measures") <<
        */
      ", line " << inputLineNumber <<
      endl <<
      endl;

    gIndenter++;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrRestMeasuresContents& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "%--> End visiting msrRestMeasuresContents" <<
      endl;
  }
#endif

  int inputLineNumber =
    elt->getInputLineNumber ();

  if (gLilypondOptions->fComments) {
    fLilypondCodeIOstream << left <<
      setw (commentFieldWidth) <<
      "% end of rest measures contents " <<
      /* JMI
      singularOrPlural (
        restMeasuresNumber,
        "measure",
        "measures") <<
        */
      ", line " << inputLineNumber <<
      endl <<
      endl;

    gIndenter--;
  }
}

//________________________________________________________________________
void lpsr2LilypondTranslator::visitStart (S_msrMidi& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> Start visiting msrMidi" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif

  if (gLilypondOptions->fNoMidi) {
    fLilypondCodeIOstream <<
      "%{" <<
      endl;

     gIndenter++;
  }

  fLilypondCodeIOstream <<
    "\\midi" << " {" <<
    endl;

  gIndenter++;

  if (gLilypondOptions->fNoMidi) {
    fLilypondCodeIOstream <<
      "% ";
  }

  fLilypondCodeIOstream <<
    "\\tempo " <<
    elt->getMidiTempoDuration () <<
    " = " <<
    elt->getMidiTempoPerSecond () <<
    endl;

  gIndenter--;

  fLilypondCodeIOstream <<
    "}" <<
    endl;

  if (gLilypondOptions->fNoMidi) {
    gIndenter--;

    fLilypondCodeIOstream <<
      "%}" <<
      endl;
  }
}

void lpsr2LilypondTranslator::visitEnd (S_msrMidi& elt)
{
#ifdef TRACE_OPTIONS
  if (gLpsrOptions->fTraceLpsrVisitors) {
    fLilypondCodeIOstream <<
      "% --> End visiting msrMidi" <<
      ", line " << elt->getInputLineNumber () <<
      endl;
  }
#endif
}


} // namespace


        /* dotted-note: JMI
  \relative {
  \override Staff.TimeSignature.stencil = #(lambda (grob)
                                             (grob-interpret-markup grob #{
                                               \markup\override #'(baseline-skip . 0.5) {
                                                 \column { \number 2 \tiny\note #"4." #-1 }
                                                 \vcenter "="
                                                 \column { \number 6 \tiny\note #"8" #-1 }
                                                 \vcenter "="
                                                 \column\number { 6 8 }
                                               }
                                               #}))
                                               *
     * ou plus simle
     * #(define-public (format-time-sig-note grob)
  (let* ((frac (ly:grob-property grob 'fraction))
  (num (if (pair? frac) (car frac) 4))
  (den (if (pair? frac) (cdr frac) 4))
  (m (markup #:override '(baseline-skip . 0.5)
  #:center-column (#:number (number->string num)
  #'" "
  #:override '(style . default)
  #:note (number->string den) DOWN))))
  (grob-interpret-markup grob m)))

         */
