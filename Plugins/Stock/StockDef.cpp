#include "pch.h"
#include "StockDef.h"
#include <iomanip>
#include "Common.h"
#include <DataManager.h>
#include <Stock.h>
#include "utilities/yyjson/yyjson.h"
#include "utilities/Common.h"
#include "utilities/JsonHelper.h"

// 美股数据长度
constexpr auto _DATA_LEN_MG = 36;
// 上证A股 & 指数数据长度
constexpr auto _DATA_LEN_SH = 34;
// 深证A股 & 指数数据长度
constexpr auto _DATA_LEN_SZ = 33;
// 北证A股 & 指数数据长度
constexpr auto _DATA_LEN_BJ = 39;
// 港股数据长度
constexpr auto _DATA_LEN_HK = 25;
// 期货数据长度(新浪 hf_ 格式，如 hf_XAU)
constexpr auto _DATA_LEN_HF = 14;
// 内盘期货数据长度(新浪 nf_ 格式：名称,提供商,开,高,低,昨收,买,卖,最新,结算,昨结,买量,卖量,持仓,成交量,...)
constexpr auto _DATA_LEN_NF = 15;

using namespace STOCK;

void STOCK::StockMarket::LoadRealtimeDataByJson(std::string json)
{
  ClearRealtimeData();

  if (json == "")
  {
    CCommon::WriteLog("Response is EMPTY!", g_data.m_log_path.c_str());
    return;
  }

  std::vector<std::string> lines = CCommon::split(CCommon::removeChar(json, '\n'), ";");
  if (lines.size() < 1)
  {
    CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
    return;
  }

  for (std::string line : lines)
  {
    if (line.empty())
    {
      continue;
    }
    line = CCommon::removeChar(CCommon::removeStr(line, "var hq_str_"), '\"');

    std::vector<std::string> item_arr = CCommon::split(line, '=');
    if (item_arr.size() <= 0)
    {
      CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
      continue;
    }

    std::wstring key = CCommon::StrToUnicode(item_arr[0].c_str());
    auto stockData = getStock(key);
    stockData->info.code = CCommon::StrToUnicode(item_arr[0].c_str());

    stockData->info.is_ok = item_arr.size() >= 2;

    if (!stockData->info.is_ok)
    {
      CCommon::WriteLog("json is INVALID!", g_data.m_log_path.c_str());
      continue;
    }

    std::string data = item_arr[1];

    std::vector<std::string> data_arr = CCommon::split(data, ",");

    stockData->info.displayName = CCommon::StrToUnicode(data_arr[0].c_str());
    stockData->realTimeData.Load(key, data_arr);
    // 期货(hf_)：名称在最后一列(索引13)，覆盖默认首列
    if (key.find(kHF) == 0 && data_arr.size() >= _DATA_LEN_HF)
    {
      stockData->info.displayName = CCommon::StrToUnicode(data_arr[13].c_str());
    }
  }
}

void STOCK::RealTimeData::Load(std::wstring key, std::vector<std::string> data_arr)
{
  size_t data_size = static_cast<size_t>(data_arr.size());

  if (key.find(kMG) == 0)
  {
    LoadMG(data_arr, data_size);
  }
  else if (key.find(kSH) == 0)
  {
    LoadSH(data_arr, data_size);
  }
  else if (key.find(kSZ) == 0)
  {
    LoadSZ(data_arr, data_size);
  }
  else if (key.find(kBJ) == 0)
  {
    LoadBJ(data_arr, data_size);
  }
  else if (key.find(kHK) == 0)
  {
    LoadHK(data_arr, data_size);
  }
  else if (key.find(kNF) == 0)
  {
    LoadNF(data_arr, data_size);
  }
  else
  {
    // hf_ 及未匹配类型(如 gds_AUTD)均按新浪期货/贵金属同格式解析
    LoadHF(data_arr, data_size);
  }

  if (currentPrice > 0 && prevClosePrice > 0)
  {
    char buff[32];
    sprintf_s(buff, "%.2f", currentPrice);
    displayPrice = CCommon::StrToUnicode(buff);

    sprintf_s(buff, "%.2f%%", ((currentPrice - prevClosePrice) / prevClosePrice * 100));
    displayFluctuation = CCommon::StrToUnicode(buff);
  }
}

void STOCK::RealTimeData::LoadMG(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_MG)
  {
    return;
  }
  openPrice = {convert<Price>(data[5])};
  prevClosePrice = {convert<Price>(data[26])};
  currentPrice = {convert<Price>(data[1])};
  highPrice = {convert<Price>(data[6])};
  lowPrice = {convert<Price>(data[7])};
  volume = {convert<Volume>(data[10])};
  // turnover = {convert<Price>(data[5])};
}

void STOCK::RealTimeData::LoadAG(std::vector<std::string> data, size_t size)
{
  openPrice = {convert<Price>(data[1])};
  prevClosePrice = {convert<Price>(data[2])};
  currentPrice = {convert<Price>(data[3])};
  highPrice = {convert<Price>(data[4])};
  lowPrice = {convert<Price>(data[5])};
  volume = {convert<Volume>(data[8])};
  turnover = {convert<Amount>(data[9])};

  Price upperLimit = abs(highPrice - prevClosePrice);
  Price lowerLimit = abs(lowPrice - prevClosePrice);

  priceLimit = max(upperLimit, lowerLimit);

  // 设置买卖盘数据
  askLevels[4] = {{convert<Price>(data[29])}, {convert<Volume>(data[28])}};
  askLevels[3] = {{convert<Price>(data[27])}, {convert<Volume>(data[26])}};
  askLevels[2] = {{convert<Price>(data[25])}, {convert<Volume>(data[24])}};
  askLevels[1] = {{convert<Price>(data[23])}, {convert<Volume>(data[22])}};
  askLevels[0] = {{convert<Price>(data[21])}, {convert<Volume>(data[20])}};

  bidLevels[0] = {{convert<Price>(data[11])}, {convert<Volume>(data[10])}};
  bidLevels[1] = {{convert<Price>(data[13])}, {convert<Volume>(data[12])}};
  bidLevels[2] = {{convert<Price>(data[15])}, {convert<Volume>(data[14])}};
  bidLevels[3] = {{convert<Price>(data[17])}, {convert<Volume>(data[16])}};
  bidLevels[4] = {{convert<Price>(data[19])}, {convert<Volume>(data[18])}};
}

void STOCK::RealTimeData::LoadSH(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_SH)
  {
    return;
  }
  LoadAG(data, size);
}

void STOCK::RealTimeData::LoadSZ(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_SZ)
  {
    return;
  }
  LoadAG(data, size);
}

void STOCK::RealTimeData::LoadBJ(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_BJ)
  {
    return;
  }
  LoadAG(data, size);
}

void STOCK::RealTimeData::LoadHK(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_HK)
  {
    return;
  }
  openPrice = {convert<Price>(data[2])};
  prevClosePrice = {convert<Price>(data[3])};
  currentPrice = {convert<Price>(data[6])};
  highPrice = {convert<Price>(data[4])};
  lowPrice = {convert<Price>(data[5])};
  volume = {convert<Volume>(data[12])};
  turnover = {convert<Price>(data[11])};
}

// 新浪内盘期货(nf_)格式: 名称,提供商,开盘,最高,最低,昨收,买价,卖价,最新,结算,昨结,买量,卖量,持仓,成交量,...
void STOCK::RealTimeData::LoadNF(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_NF)
  {
    return;
  }
  openPrice = {convert<Price>(data[2])};
  highPrice = {convert<Price>(data[3])};
  lowPrice = {convert<Price>(data[4])};
  prevClosePrice = {convert<Price>(data[5])};
  currentPrice = {convert<Price>(data[8])};
  volume = {convert<Volume>(data[14])};
  Price upperLimit = (prevClosePrice > 0) ? (highPrice - prevClosePrice) : (highPrice - lowPrice);
  Price lowerLimit = (prevClosePrice > 0) ? (prevClosePrice - lowPrice) : (highPrice - lowPrice);
  priceLimit = (std::max)(upperLimit, lowerLimit);
  if (priceLimit <= 0.0)
    priceLimit = (highPrice - lowPrice) > 0 ? (highPrice - lowPrice) : 0.01;
  // 买一/卖一
  bidLevels[0] = {{convert<Price>(data[6])}, {convert<Volume>(data[11])}};
  askLevels[0] = {{convert<Price>(data[7])}, {convert<Volume>(data[12])}};
}

// 新浪期货(hf_)/贵金属(gds_)格式: 现价,昨收,今开,?,最高,最低,时间,...,日期,名称；gds 昨收常为 0，用今开作参考
void STOCK::RealTimeData::LoadHF(std::vector<std::string> data, size_t size)
{
  if (size < _DATA_LEN_HF)
  {
    return;
  }
  openPrice = {convert<Price>(data[2])};
  prevClosePrice = {convert<Price>(data[1])};
  currentPrice = {convert<Price>(data[0])};
  highPrice = {convert<Price>(data[4])};
  lowPrice = {convert<Price>(data[5])};
  volume = {convert<Volume>(data[10])};
  turnover = {convert<Amount>(data[11])};
  // gds_AUTD 等接口昨收常为 0，用今开作为涨跌幅参考
  if (prevClosePrice <= 0.0 && openPrice > 0.0)
    prevClosePrice = openPrice;
}

void STOCK::StockMarket::LoadTimelineDataByJson(std::wstring stock_id, CString *pData)
{
  auto data = g_data.GetStockData(stock_id);
  {
    std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
    data->clearTimelinePoint();
    if (pData)
    {
      data->addTimelinePoint(*pData);
    }
  }
  Stock::Instance().UpdateKLine();
}

void STOCK::StockMarket::LoadTimelineDataByJsonNf(std::wstring stock_id, CString *pData)
{
  auto data = g_data.GetStockData(stock_id);
  {
    std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
    data->clearTimelinePoint();
    if (pData)
    {
      data->addTimelinePointFromFuturesJson(*pData);
    }
  }
  Stock::Instance().UpdateKLine();
}

std::wstring STOCK::StockData::GetCurrentDisplay(bool include_name) const
{
  std::wstringstream wss;
  if (info.is_ok)
  {
    if (include_name)
      wss << info.displayName << ": ";
    wss << realTimeData.displayPrice << ' ';
  }
  else
  {
    wss << info.code + L" " + g_data.StringRes(IDS_LOAD_FAIL).GetString();
  }
  return wss.str();
}

static Volume GetJsonVolume(yyjson_val *obj, const char *key)
{
  if (obj != nullptr)
  {
    yyjson_val *val = yyjson_obj_get(obj, key);
    try
    {

      if (val != nullptr)
      {
        if (yyjson_is_uint(val))
        {
          return static_cast<Volume>(yyjson_get_uint(val));
        }
        else if (yyjson_is_str(val))
        {
          return static_cast<Volume>(std::stoul(yyjson_get_str(val)));
        }
      }
    }
    catch (const std::exception &e)
    {
      return 0L;
    }
  }
  return 0L;
}

static Price GetJsonPrice(yyjson_val *obj, const char *key)
{
  if (obj != nullptr)
  {
    yyjson_val *val = yyjson_obj_get(obj, key);
    try
    {
      if (val != nullptr)
      {
        if (yyjson_is_real(val))
        {
          return static_cast<Price>(yyjson_get_real(val));
        }
        else if (yyjson_is_str(val))
        {
          return static_cast<Price>(std::stod(yyjson_get_str(val)));
        }
      }
    }
    catch (const std::invalid_argument &e)
    {
      return 0.0F;
    }
    catch (const std::out_of_range &e)
    {
      return 0.0F;
    }
  }
  return 0.0F;
}

void STOCK::StockData::addTimelinePoint(const CString &json_data)
{
  std::string _json_data = CCommon::UnicodeToStr(json_data);
  yyjson_doc *doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
  if (doc != nullptr)
  {
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == nullptr)
    {
      return;
    }

    yyjson_val *result = yyjson_obj_get(root, "result");
    if (result == nullptr)
    {
      return;
    }

    yyjson_val *data = yyjson_obj_get(result, "data");
    if (data != nullptr && yyjson_is_arr(data))
    {
      yyjson_val *item;
      yyjson_arr_iter iter;
      yyjson_arr_iter_init(data, &iter);
      while ((item = yyjson_arr_iter_next(&iter)))
      {
        if (item != nullptr)
        {
          TimelinePoint point = TimelinePoint();
          point.time = utilities::JsonHelper::GetJsonString(item, "m");
          point.volume = GetJsonVolume(item, "v");
          point.price = GetJsonPrice(item, "p");
          point.averagePrice = GetJsonPrice(item, "avg_p");
          addTimelinePoint(point);
        }
      }
    }
  }
}

// 新浪内盘期货 K 线接口返回: [[时间,开,高,低,收,量],...] 按时间倒序(新在前)，转为分时点并反转为时间正序(旧在前)便于绘图
void STOCK::StockData::addTimelinePointFromFuturesJson(const CString &json_data)
{
  std::string _json_data = CCommon::UnicodeToStr(json_data);
  yyjson_doc *doc = yyjson_read(_json_data.c_str(), _json_data.size(), 0);
  if (doc == nullptr)
    return;
  yyjson_val *root = yyjson_doc_get_root(doc);
  if (root == nullptr || !yyjson_is_arr(root))
  {
    yyjson_doc_free(doc);
    return;
  }
  std::vector<TimelinePoint> points;
  size_t idx, max;
  yyjson_val *item;
  yyjson_arr_foreach(root, idx, max, item)
  {
    if (item == nullptr || !yyjson_is_arr(item) || yyjson_arr_size(item) < 6)
      continue;
    yyjson_val *v0 = yyjson_arr_get(item, 0);
    yyjson_val *v1 = yyjson_arr_get(item, 1);
    yyjson_val *v2 = yyjson_arr_get(item, 2);
    yyjson_val *v3 = yyjson_arr_get(item, 3);
    yyjson_val *v4 = yyjson_arr_get(item, 4);
    yyjson_val *v5 = yyjson_arr_get(item, 5);
    if (v0 == nullptr || v1 == nullptr || v4 == nullptr || v5 == nullptr)
      continue;
    const char *time_str = yyjson_get_str(v0);
    Price open_p = 0, high_p = 0, low_p = 0, close_p = 0;
    Volume vol = 0;
    if (time_str == nullptr)
      continue;
    if (yyjson_is_str(v1))
      open_p = static_cast<Price>(std::stod(yyjson_get_str(v1)));
    if (v2 && yyjson_is_str(v2))
      high_p = static_cast<Price>(std::stod(yyjson_get_str(v2)));
    if (v3 && yyjson_is_str(v3))
      low_p = static_cast<Price>(std::stod(yyjson_get_str(v3)));
    if (yyjson_is_str(v4))
      close_p = static_cast<Price>(std::stod(yyjson_get_str(v4)));
    if (yyjson_is_str(v5))
      vol = static_cast<Volume>(std::stoul(yyjson_get_str(v5)));
    TimelinePoint point;
    point.time = time_str;
    point.price = close_p;
    point.volume = vol;
    point.averagePrice = (open_p + high_p + low_p + close_p) / 4.0;
    points.push_back(point);
  }
  yyjson_doc_free(doc);
  // 接口返回新→旧，绘图需要旧→新，故反向添加
  for (auto it = points.rbegin(); it != points.rend(); ++it)
    addTimelinePoint(*it);
}
