#include "StdAfx.h"
#include "BasketTradeModel.h"

#include "DbValueStream.h"
#include "CommonDatabaseFunctions.h"

#include <ostream>

//
// CBasketTradeModel
//

CBasketTradeModel::CBasketTradeModel( CProviderInterface *pDataProvider, CProviderInterface *pExecutionProvider )
: m_pDataProvider( pDataProvider ), m_pExecutionProvider( pExecutionProvider )
{
}

CBasketTradeModel::~CBasketTradeModel(void) {
  mapBasketSymbols_t::iterator iter;
  for ( iter = m_mapBasketSymbols.begin(); iter != m_mapBasketSymbols.end(); ++iter ) {
    // todo: need to remove any events attached to this object as well
    delete iter->second;
  }
  m_mapBasketSymbols.clear();
}

void CBasketTradeModel::AddSymbol(const std::string &sSymbolName, const std::string &sPath, const std::string &sStrategy) {
  mapBasketSymbols_t::iterator iter;
  iter = m_mapBasketSymbols.find( sSymbolName );
  if ( m_mapBasketSymbols.end() == iter ) {
    CBasketTradeSymbolInfo *pInfo = new CBasketTradeSymbolInfo( sSymbolName, sPath, sStrategy, m_pExecutionProvider );
    m_mapBasketSymbols.insert( pairBasketSymbolsEntry_t( sSymbolName, pInfo ) );
    OnBasketTradeSymbolInfoAddedToBasket( pInfo );
    std::cout << "Basket add for " << sSymbolName << " successful." << std::endl;
  }
  else {
    std::cout << "Basket add for " << sSymbolName << " failed, symbol already exists." << std::endl;
  }
}

void CBasketTradeModel::Prepare( ptime dtTradeDate, double dblFunds, bool bRTHOnly ) {
  int nTradeableSymbols = 0;
  int nSymbolCount = 0;
  double dblFundsPerSymbol = 0;
  double dblCostForEntry = 0;
  mapBasketSymbols_t::iterator iter; 
  try {
    for ( int nLoopCount = 1; nLoopCount <= 2; ++nLoopCount ) {
      double dblTotalCostForEntry = 0;
      switch ( nLoopCount ) {
        case 1:
          nSymbolCount = m_mapBasketSymbols.size();
          dblFundsPerSymbol = dblFunds / nSymbolCount;
          break;
        case 2: 
          nSymbolCount = nTradeableSymbols;
          dblFundsPerSymbol = dblFunds / nSymbolCount;
          break;
      }
      for( iter = m_mapBasketSymbols.begin(); iter != m_mapBasketSymbols.end(); ++iter ) {
        switch ( nLoopCount ) {
          case 1:
            iter->second->CalculateTrade( dtTradeDate, dblFundsPerSymbol, bRTHOnly );
            dblCostForEntry = iter->second->GetProposedEntryCost();
            if ( 0 != dblCostForEntry ) {
              ++nTradeableSymbols;
              dblTotalCostForEntry += dblCostForEntry;
            }
            break;
          case 2:
            dblCostForEntry = iter->second->GetProposedEntryCost();  // was set on last loop through
            if ( 0 != dblCostForEntry ) {
              iter->second->CalculateTrade( dtTradeDate, dblFundsPerSymbol, bRTHOnly );
              dblCostForEntry = iter->second->GetProposedEntryCost();
              dblTotalCostForEntry += dblCostForEntry;
              m_pDataProvider->AddTradeHandler( iter->second->GetSymbolName(), MakeDelegate( iter->second, &CBasketTradeSymbolInfo::HandleTrade ) );
              m_pDataProvider->AddQuoteHandler( iter->second->GetSymbolName(), MakeDelegate( iter->second, &CBasketTradeSymbolInfo::HandleQuote ) );
              m_pDataProvider->AddOnOpenHandler( iter->second->GetSymbolName(), MakeDelegate( iter->second, &CBasketTradeSymbolInfo::HandleOpen ) );
            }
            break;
        }
      }
      std::cout << "# Symbols: " << nSymbolCount << ", Total Cost of Entry: " << dblTotalCostForEntry << std::endl;
    }
  }
  catch (...) {
    std::cout << "error somewhere" << std::endl;
  }
}

void CBasketTradeModel::WriteBasketToDatabase() {

  CDbValueStream strm;
  std::ostream out(&strm);
  unsigned long key = 0;

  strm.Truncate();
  for( mapBasketSymbols_t::iterator iter = m_mapBasketSymbols.begin(); iter != m_mapBasketSymbols.end(); ++iter ) {
    out.flush();
    iter->second->StreamSymbolInfo( &out );
    strm.Save( &key, sizeof( key ) );
    ++key;
  }
}

void CBasketTradeModel::ReadBasketFromDatabase() {
  std::stringstream in;
  CBasketTradeSymbolInfo *pBasket = new CBasketTradeSymbolInfo( &in, m_pExecutionProvider );
}

void CBasketTradeModel::WriteBasketData( const std::string &sPathPrefix ) {
  for( mapBasketSymbols_t::iterator iter = m_mapBasketSymbols.begin(); iter != m_mapBasketSymbols.end(); ++iter ) {
    iter->second->WriteTradesAndQuotes( sPathPrefix );
  }
}