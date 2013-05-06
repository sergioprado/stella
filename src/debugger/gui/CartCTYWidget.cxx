//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2013 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id$
//============================================================================

#include "CartCTY.hxx"
#include "PopUpWidget.hxx"
#include "CartCTYWidget.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
CartridgeCTYWidget::CartridgeCTYWidget(
      GuiObject* boss, const GUI::Font& font,
      int x, int y, int w, int h, CartridgeCTY& cart)
  : CartDebugWidget(boss, font, x, y, w, h),
    myCart(cart)
{
  uInt16 size = 8 * 4096;

  string info =
    "Chetiry cartridge, eight 4K banks (bank 0 is ARM code and is ignored)\n"
    "64 bytes RAM @ $F000 - $F080\n"
    "  $F040 - $F07F (R), $F000 - $F03F (W)\n"
    "\nTHIS SCHEME IS NOT FULLY IMPLEMENTED OR TESTED\n";

  int xpos = 10,
      ypos = addBaseInformation(size, "Chris D. Walton", info) + myLineHeight;

  StringMap items;
  items.push_back("1 ($FF5)", "1");
  items.push_back("2 ($FF6)", "2");
  items.push_back("3 ($FF7)", "3");
  items.push_back("4 ($FF8)", "4");
  items.push_back("5 ($FF9)", "5");
  items.push_back("6 ($FFA)", "6");
  items.push_back("7 ($FFB)", "7");
  myBank =
    new PopUpWidget(boss, font, xpos, ypos-2, font.getStringWidth("0 ($FFx) "),
                    myLineHeight, items, "Set bank: ",
                    font.getStringWidth("Set bank: "), kBankChanged);
  myBank->setTarget(this);
  addFocusWidget(myBank);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCTYWidget::loadConfig()
{
  myBank->setSelected(myCart.bank()-1);

  CartDebugWidget::loadConfig();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void CartridgeCTYWidget::handleCommand(CommandSender* sender,
                                      int cmd, int data, int id)
{
  if(cmd == kBankChanged)
  {
    myCart.unlockBank();
    myCart.bank(myBank->getSelected()+1);
    myCart.lockBank();
    invalidate();
  }
}