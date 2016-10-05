/*
  MusicXML Library
  Copyright (C) Grame 2006-2013

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr
*/

#ifdef VC6
# pragma warning (disable : 4786)
#endif

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <locale>         // locale, tolower

#include "tree_browser.h"
#include "xml_tree_browser.h"

#include "rational.h"

#include "lpsrUtilities.h"
#include "xml2LpsrVisitor.h"
#include "xmlPartSummaryVisitor.h"
#include "xmlPart2LpsrVisitor.h"

using namespace std;

namespace MusicXML2
{

//______________________________________________________________________________
xml2LpsrVisitor::xml2LpsrVisitor( S_translationSettings& ts )
{
  fTranslationSettings = ts;
  
  fMillimeters     = -1;
  fGlobalStaffSize = -1.0;
  fTenths          = -1;
  
  fCurrentStaffIndex = 0;
  
  // create the header element
  fLpsrHeader = lpsrHeader::create ();

  // create the paper element
  fLpsrPaper = lpsrPaper::create ();
  
  // create the layout element
  fLpsrLayout = lpsrLayout::create ();
   
  // create the score element
  fLpsrScore = lpsrScore::create ();

   // create the implicit lpsrSequence element FIRST THING!
  fImplicitSequence = lpsrSequence::create (lpsrSequence::kEndOfLine);
    
  // append the header to the lpsrSequence
  S_lpsrElement header = fLpsrHeader;
  fImplicitSequence->appendElementToSequence (header);

  // append the paper to the lpsrSequence
  S_lpsrElement paper = fLpsrPaper;
  fImplicitSequence->appendElementToSequence (paper);

  // append the layout to the lpsrSequence
  S_lpsrElement layout = fLpsrLayout;
  fImplicitSequence->appendElementToSequence (layout);

  // add the "indent" association to the layout
  S_lpsrLilypondVarValAssoc indent =
    lpsrLilypondVarValAssoc::create (
      "indent", "0",
      lpsrLilypondVarValAssoc::kEqualSign,
      lpsrLilypondVarValAssoc::kNoQuotesAroundValue,
      lpsrLilypondVarValAssoc::kUncommented,
      "\\cm");
  fLpsrLayout->addlpsrLilypondVarValAssoc (indent);
  
  // add the "indent" association to the layout
  S_lpsrLilypondVarValAssoc pageCount =
    lpsrLilypondVarValAssoc::create (
      "page-cout", "0",
      lpsrLilypondVarValAssoc::kEqualSign,
      lpsrLilypondVarValAssoc::kNoQuotesAroundValue,
      lpsrLilypondVarValAssoc::kCommented);
  fLpsrLayout->addlpsrLilypondVarValAssoc (pageCount);
  
  // add the "indent" association to the layout
  S_lpsrLilypondVarValAssoc systemCount =
    lpsrLilypondVarValAssoc::create (
      "system-count", "0",
      lpsrLilypondVarValAssoc::kEqualSign,
      lpsrLilypondVarValAssoc::kNoQuotesAroundValue,
      lpsrLilypondVarValAssoc::kCommented);
  fLpsrLayout->addlpsrLilypondVarValAssoc (systemCount);
  
  // add standard postamble
  appendPostamble ();
  
  fVisitingPageLayout = false;
}

//______________________________________________________________________________
S_lpsrElement xml2LpsrVisitor::convertToLpsr (const Sxmlelement& xml )
{
  S_lpsrElement result;
  if (xml) {
    // create a browser on this xml2LpsrVisitor
    tree_browser<xmlelement> browser (this);
    
    // browse the xmlelement tree
    browser.browse (*xml);
    
    // the stack top contains the resulting lpsrElement tree
    result = fImplicitSequence;
  }
  return result;
}

//______________________________________________________________________________
void xml2LpsrVisitor::prependPreamble () {
  // prepending elements in reverse order

  if (fGlobalStaffSize > 0.0) {
    S_lpsrSchemeVarValAssoc staffSize =
          lpsrSchemeVarValAssoc:: create(
            "set-global-staff-size", fGlobalSfaffSizeAsString,
            lpsrSchemeVarValAssoc::kCommented);
    fImplicitSequence->prependElementToSequence (staffSize);
  
    S_lpsrComment com =
      lpsrComment::create (
        "uncomment the following to keep original score global size");
    fImplicitSequence->prependElementToSequence (com);
  }
  
  S_lpsrLilypondVarValAssoc version =
        lpsrLilypondVarValAssoc:: create(
          "\\version", "2.19",
          lpsrLilypondVarValAssoc::kSpace,
          lpsrLilypondVarValAssoc::kQuotesAroundValue,
          lpsrLilypondVarValAssoc::kUncommented);
  fImplicitSequence->prependElementToSequence (version);
}
 
void xml2LpsrVisitor::appendPostamble () {
  S_lpsrComment com1 =
    lpsrComment::create (
      "choose \\break below to keep the original line breaks");
  fImplicitSequence->appendElementToSequence (com1);

  S_lpsrLilypondVarValAssoc myBreak1 =
        lpsrLilypondVarValAssoc:: create(
          "myBreak", "{ \\break }",
          lpsrLilypondVarValAssoc::kEqualSign,
          lpsrLilypondVarValAssoc::kNoQuotesAroundValue,
          lpsrLilypondVarValAssoc::kUncommented);
  fImplicitSequence->appendElementToSequence (myBreak1);

  S_lpsrComment com2 =
    lpsrComment::create (
      "choose {} below to let lpsr determine where to break lines");
  fImplicitSequence->appendElementToSequence (com2);

  S_lpsrLilypondVarValAssoc myBreak2 =
        lpsrLilypondVarValAssoc:: create(
          "myBreak", "{}",
          lpsrLilypondVarValAssoc::kEqualSign,
          lpsrLilypondVarValAssoc::kNoQuotesAroundValue,
          lpsrLilypondVarValAssoc::kCommented);
  fImplicitSequence->appendElementToSequence (myBreak2);
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_score_timewise& elt )
{
  lpsrMusicXMLError("score-timewise MusicXML data is not supported");
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_score_partwise& elt )
{
  // store the element in the header
// JMI  fLpsrHeader->setScorePartwise(elt);
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_part& elt )
{
  string partID = elt->getAttributeValue ("id");
  
  // browse the part contents for the first time with a xmlPartSummaryVisitor
  if (fTranslationSettings->fTrace)
    cerr << "Extracting part \"" << partID << "\" summary information" << endl;

  xmlPartSummaryVisitor partSummaryVisitor (fTranslationSettings);
  
  xml_tree_browser      browser (&partSummaryVisitor);
  browser.browse (*elt);

  // create the part
  string
    partName =
      "Part" + stringNumbersToEnglishWords (partID);
  
  if (fTranslationSettings->fTrace)
    cerr << "Creating part \"" << partName << "\" (" << partID << ")" << endl;
  
  S_lpsrPart
    part =
      lpsrPart::create (
        partID,
        partName,
        fCurrentInstrumentName,
        fTranslationSettings->fGenerateAbsoluteCode,
        fTranslationSettings->fGenerateNumericalTime );
    
  // register part in this visitors's part map
  fLpsrPartsMap [partID] = part;

  if (fTranslationSettings->fTrace)
    cerr << "Getting the part voices IDs" << endl;

  int         partVoicesNumber =
                partSummaryVisitor.getPartVoicesNumber (partID);
  
  if (partVoicesNumber > 1)
    cerr << "Theare are " << partVoicesNumber << " voices";
  else
    cerr << "There is 1 voice";
  cerr << " in part " << partName << "\" (" << partID << ")" << endl;
  
  int      targetStaff = -1;
  bool     notesOnly = false;
  rational currentTimeSign (0,1);
  
  // browse the parts voice by voice: 
  // this allows to describe voices that spans over several staves
  // and to collect the voice's lyrics
    
  if (fTranslationSettings->fTrace)
    cerr << "Extracting part \"" << partID << "\" voices information" << endl;

  if (fTranslationSettings->fTrace) {
    if (partVoicesNumber > 1)
      cerr << "Theare are " << partVoicesNumber << " voices";
    else
      cerr << "There is 1 voice";
    cerr << "  in part" << partID << endl;
  }
  
  vector<int>
    partVoicesIDs =
      partSummaryVisitor.getPartVoicesIDs (partID);

  if (fTranslationSettings->fTrace)
    cerr << "--> partVoicesIDs.size() = " << partVoicesIDs.size() << endl;

  for (unsigned int i = 0; i < partVoicesIDs.size(); i++) {

    int voiceID     = i;
    int targetVoice = partVoicesIDs [i];
    
    string
      voiceName =
        partName + "_Voice" + int2EnglishWord (targetVoice);
      
    int targetVoiceMainStaffID =
          partSummaryVisitor.getVoiceMainStaffID (targetVoice);
  
    if (targetStaff == targetVoiceMainStaffID) {
      notesOnly = true;
    }
    else {
      notesOnly = false;
      targetStaff = targetVoiceMainStaffID;
      fCurrentStaffIndex++;
    }

    int partVoiceLyricsNumber =
          partSummaryVisitor.getPartVoiceLyricsNumber (partID, voiceID);

    if (fTranslationSettings->fTrace)
      cerr << 
        "Handling part \"" << partID << 
        "\" contents, targetVoiceMainStaffID = " << targetVoiceMainStaffID <<
        ", targetStaff = " << targetStaff <<
        ", targetVoice = " << targetVoice << endl;

    // create the voice
    if (fTranslationSettings->fTrace)
      cerr << "Creating voice \"" << voiceName << "\" in part " << partName << endl;

    S_lpsrVoice
      voice =
        lpsrVoice::create (
          voiceName,
          fTranslationSettings->fGenerateAbsoluteCode,
          fTranslationSettings->fGenerateNumericalTime);

    // register the voice in the part
    part->addVoiceToPart (targetVoice, voice);
    
    // append the voice to the part sequence
    S_lpsrElement elem = voice;
    fImplicitSequence->appendElementToSequence (elem);

    // browse the part contents once more with an xmlPart2LpsrVisitor
    xmlPart2LpsrVisitor
      xp2lv (
        fTranslationSettings,
        fLpsrScore,
        fImplicitSequence,
        part,
        voice,
        targetStaff,
        fCurrentStaffIndex,
        targetVoice,
        notesOnly,
        currentTimeSign);
    xml_tree_browser browser (&xp2lv);
    browser.browse (*elt);

    // JMI currentTimeSign = xp2lv.getTimeSign();

    // extract the part lyrics
  //  if (fTranslationSettings->fTrace)
  // JMI    cerr << "Extracting part \"" << partID << "\" lyrics information" << endl;

  } // for
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitEnd ( S_score_partwise& elt )
{
  // now we can insert the global staff size where needed
  stringstream s;

  s << fGlobalStaffSize;
  s >> fGlobalSfaffSizeAsString;
  
  // add standard preamble ahead of the part element sequence
  // only now because set-global-staff-size needs information
  // from the <scaling> element
  prependPreamble ();

  // get score layout
  S_lpsrLayout
    layout =
      fLpsrScore->getScoreLayout();

  if (fGlobalStaffSize > 0.0) {
    S_lpsrSchemeVarValAssoc
      staffSize =
        lpsrSchemeVarValAssoc::create (
          "layout-set-staff-size", fGlobalSfaffSizeAsString,
          lpsrSchemeVarValAssoc::kCommented);
    layout->addLpsrSchemeVarValAssoc (staffSize);  
  }

  if (fTranslationSettings->fTrace) {
    int size = fLpsrPartsMap.size();

    if (size > 1)
      cerr << "Theare are " << size << " parts";
    else
      cerr << "There is 1 part";
    cerr << " in the score" << endl;
  }

  // add the voices and lyrics to the score parallel music
  lpsrPartsMap::const_iterator i;
  for (i = fLpsrPartsMap.begin(); i != fLpsrPartsMap.end(); i++) {
    
    // get part and part name
    S_lpsrPart part     = (*i).second;
    string     partName = part->getPartName ();
    string     partID   = part->getPartID();
     
    // create a staff
    cout << "--> creating a new staff command" << endl;
    S_lpsrNewstaffCommand
      newStaffCommand =
        lpsrNewstaffCommand::create();
    
    // get the part voices
    map<int, S_lpsrVoice>
      partVoicesMap =
        part->getPartVoicesMap ();
 
    // add the voices lyrics to the staff command
    int voicesNbr = partVoicesMap.size();

    if (voicesNbr > 1)
      cerr << "Theare are " << voicesNbr << " voices";
    else
      cerr << "There is 1 voice";
    cerr << " in part " << partName << " (" << partID << ") BOF" << endl;

    map<int, S_lpsrVoice>::const_iterator i;
    for (i = partVoicesMap.begin(); i != partVoicesMap.end(); i++) {

      S_lpsrVoice voice = (*i).second;
      string voiceName  = voice->getVoiceName();

      // create the voice context
      cout << "--> creating voice " << voiceName << endl;
      S_lpsrContext
        voiceContext =
          lpsrContext::create (
            lpsrContext::kNewContext, "Voice", voiceName);
          
      // add a use of the part name to the voice
      S_lpsrVariableUseCommand
        variableUse =
          lpsrVariableUseCommand::create (voiceName);
      voiceContext->addElementToContext (variableUse);
  
      // add the voice to the staff
      newStaffCommand->addElementToNewStaff (voiceContext);
    } // for
    
    // add the new staff to the score parallel music
    cout << "--> add the new staff to the score parallel music" << endl;
    S_lpsrParallelMusic
      scoreParallelMusic =
        fLpsrScore->getScoreParallelMusic ();
    scoreParallelMusic->addElementToParallelMusic (newStaffCommand);


    /*
       \new Staff <<
            \set Staff.instrumentName = "Violins 1"
            \context Staff << 
                \context Voice = "PartPOneVoiceOne" { \voiceOne \PartPOneVoiceOne }
                \new Lyrics \lyricsto "PartPOneVoiceOne" \PartPOneVoiceOneLyricsOne
                \new Lyrics \lyricsto "PartPOneVoiceOne" \PartPOneVoiceOneLyricsTwo
                \new Lyrics \lyricsto "PartPOneVoiceOne" \PartPOneVoiceOneLyricsThree
                \new Lyrics \lyricsto "PartPOneVoiceOne" \PartPOneVoiceOneLyricsFour
                \new Lyrics \lyricsto "PartPOneVoiceOne" \PartPOneVoiceOneLyricsFive
                \new Lyrics \lyricsto "PartPOneVoiceOne" \PartPOneVoiceOneLyricsSix
                \context Voice = "PartPOneVoiceTwo" { \voiceTwo \PartPOneVoiceTwo }
                >>
            >>
      */
  } // for
  
  // append the score to the implicit Sequence
  // only now to place it after the postamble
  S_lpsrElement
    score = fLpsrScore;
  fImplicitSequence->appendElementToSequence (score);
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_work_number& elt )
  { fLpsrHeader->setWorkNumber (elt->getValue()); }

void xml2LpsrVisitor::visitStart ( S_work_title& elt )
  { fLpsrHeader->setWorkTitle (elt->getValue()); }
  
void xml2LpsrVisitor::visitStart ( S_movement_number& elt )
  { fLpsrHeader->setMovementNumber (elt->getValue()); }

void xml2LpsrVisitor::visitStart ( S_movement_title& elt )
  { fLpsrHeader->setMovementTitle (elt->getValue()); }

void xml2LpsrVisitor::visitStart ( S_creator& elt )
{
  string type = elt->getAttributeValue ("type");
  fLpsrHeader->addCreator (type, elt->getValue());
}

void xml2LpsrVisitor::visitStart ( S_rights& elt )
  { fLpsrHeader->setRights (elt->getValue()); }

void xml2LpsrVisitor::visitStart ( S_software& elt )
  { fLpsrHeader->addSoftware (elt->getValue()); }

void xml2LpsrVisitor::visitStart ( S_encoding_date& elt )
  { fLpsrHeader->setEncodingDate (elt->getValue()); }

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_millimeters& elt )
{ 
  fMillimeters = (int)(*elt);
//  cout << "--> fMillimeters = " << fMillimeters << endl;
  
  fGlobalStaffSize = fMillimeters*72.27/25.4;
//  cout << "--> fGlobalStaffSize = " << fGlobalStaffSize << endl;
}

void xml2LpsrVisitor::visitStart ( S_tenths& elt )
{
  fTenths = (int)(*elt);
//  cout << "--> fTenths = " << fTenths << endl;
}

void xml2LpsrVisitor::visitEnd ( S_scaling& elt)
{
  if (fTranslationSettings->fTrace)
    cerr <<
      "There are " << fTenths << " tenths for " << 
      fMillimeters << " millimeters, hence a global staff size of " <<
      fGlobalStaffSize << endl;
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_system_distance& elt )
{
  int systemDistance = (int)(*elt);
//  cout << "--> systemDistance = " << systemDistance << endl;
  fLpsrPaper->setBetweenSystemSpace (1.0*systemDistance*fMillimeters/fTenths/10);  
}
void xml2LpsrVisitor::visitStart ( S_top_system_distance& elt )
{
  int topSystemDistance = (int)(*elt);
//  cout << "--> fTopSystemDistance = " << topSystemDistance << endl;
  fLpsrPaper->setPageTopSpace (1.0*topSystemDistance*fMillimeters/fTenths/10);  
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_page_layout& elt )
{
  fVisitingPageLayout = true;
}
void xml2LpsrVisitor::visitEnd ( S_page_layout& elt )
{
  fVisitingPageLayout = false;
}

void xml2LpsrVisitor::visitStart ( S_page_height& elt )
{
  if (fVisitingPageLayout) {
    int pageHeight = (int)(*elt);
    //cout << "--> pageHeight = " << pageHeight << endl;
    fLpsrPaper->setPaperHeight (1.0*pageHeight*fMillimeters/fTenths/10);  
  }
}
void xml2LpsrVisitor::visitStart ( S_page_width& elt )
{
  if (fVisitingPageLayout) {
    int pageWidth = (int)(*elt);
    //cout << "--> pageWidth = " << pageWidth << endl;
    fLpsrPaper->setPaperWidth (1.0*pageWidth*fMillimeters/fTenths/10);
  }
}

void xml2LpsrVisitor::visitStart ( S_left_margin& elt )
{
  if (fVisitingPageLayout) {
    int leftMargin = (int)(*elt);
    //cout << "--> leftMargin = " << leftMargin << endl;
    fLpsrPaper->setLeftMargin (1.0*leftMargin*fMillimeters/fTenths/10);  
  }
}
void xml2LpsrVisitor::visitStart ( S_right_margin& elt )
{
  if (fVisitingPageLayout) {
    int rightMargin = (int)(*elt);
    //cout << "--> rightMargin = " << rightMargin << endl;
    fLpsrPaper->setRightMargin (1.0*rightMargin*fMillimeters/fTenths/10);  
  }
}
void xml2LpsrVisitor::visitStart ( S_top_margin& elt )
{
  if (fVisitingPageLayout) {
    int topMargin = (int)(*elt);
    //cout << "--> topMargin = " << topMargin << endl;
    fLpsrPaper->setTopMargin (1.0*topMargin*fMillimeters/fTenths/10);  
  }
}
void xml2LpsrVisitor::visitStart ( S_bottom_margin& elt )
{
  if (fVisitingPageLayout) {
    int bottomMargin = (int)(*elt);
    //cout << "--> bottomMargin = " << bottomMargin << endl;
    fLpsrPaper->setBottomMargin (1.0*bottomMargin*fMillimeters/fTenths/10);  
  }
}

//______________________________________________________________________________
void xml2LpsrVisitor::visitStart ( S_instrument_name& elt )
  { fCurrentInstrumentName = elt->getValue(); }

void xml2LpsrVisitor::visitStart ( S_score_part& elt )
  { fCurrentPartID = elt->getAttributeValue("id"); }

void xml2LpsrVisitor::visitStart ( S_part_name& elt )
  { fCurrentPartName = elt->getValue(); }

//______________________________________________________________________________
void xml2LpsrVisitor::addPosition ( 
  Sxmlelement elt, S_lpsrElement& cmd, int yoffset)
{
  float posx = elt->getAttributeFloatValue("default-x", 0) + elt->getAttributeFloatValue("relative-x", 0);
  if (posx) {
    posx = (posx / 10) * 2;   // convert to half spaces
    stringstream s;
    s << "dx=" << posx << "hs";
  }
  
  float posy = elt->getAttributeFloatValue("default-y", 0) + elt->getAttributeFloatValue("relative-y", 0);
  if (posy) {
    posy = (posy / 10) * 2;   // convert to half spaces
    posy += yoffset;      // anchor point convertion (defaults to upper line in xml)
    stringstream s;
    s << "dy=" << posy << "hs";
  }
}


}


/*
   instrument
     
  staccato
  * 
  * 
  * accent
  * strong-accent
tenuto

breath-mark
* 
* coda
creator

ending
fermata

grace
* 
* 
* octave
octave-shift
* 

sign
* 
* time-modification
type
unpitched
voice


  */

