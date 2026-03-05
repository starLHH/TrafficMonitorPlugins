#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RequestRealtimeData 测试脚本（Python 版）
模拟 DataManager::RequestRealtimeData() 的请求逻辑，输出调用参数和接口信息。

运行：
  python test_request_realtime_data.py
  python test_request_realtime_data.py sz000001 sh600000
"""
import random
import sys
import urllib.request
import urllib.error

# 与 DataManager 中一致的 User-Agent
WEB_USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/135.0.0.0 Safari/537.36 Edg/135.0.0.0"
# 接口基地址（新浪行情）
API_BASE = "https://hq.sinajs.cn/?"
REFERER = "https://finance.sina.com.cn"

# 默认股票代码（与 C++ 版一致）
DEFAULT_CODES = ["sz000759", "hf_XAU", "gds_AUTD"]


def build_url(codes):
    """构造与 RequestRealtimeData 一致的 URL：_=随机数&list=代码1,代码2"""
    rand_val = random.random()
    list_param = ",".join(codes)
    url = f"{API_BASE}_={rand_val}&list={list_param}"
    return url, rand_val, list_param


def http_get(url, referer):
    """发 GET 请求，返回 (成功与否, 响应 body 字符串)"""
    req = urllib.request.Request(url, method="GET")
    req.add_header("User-Agent", WEB_USER_AGENT)
    req.add_header("Referer", referer)
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            body = resp.read()
            # 新浪返回多为 GBK
            try:
                return True, body.decode("gbk")
            except UnicodeDecodeError:
                return True, body.decode("utf-8", errors="replace")
    except urllib.error.URLError as e:
        return False, str(e)


def main():
    codes = sys.argv[1:] if len(sys.argv) > 1 else DEFAULT_CODES
    url, rand_val, list_param = build_url(codes)

    print("========== RequestRealtimeData 测试 ==========\n")
    print("[接口名称] 新浪股票实时行情")
    print("[接口地址]", API_BASE)
    print("[请求方式] GET\n")
    print("[调用参数]")
    print("  _    =", rand_val, "  (防缓存随机数)")
    print("  list =", list_param, "  (股票代码，逗号分隔)\n")
    print("[完整 URL]")
    print(" ", url, "\n")
    print("[请求头]")
    print("  User-Agent:", WEB_USER_AGENT)
    print("  Referer:", REFERER, "\n")

    print("[请求结果]")
    ok, body = http_get(url, REFERER)
    if not ok:
        print("  HTTP 请求失败:", body)
        return 1
    print("  状态: 成功")
    print("  响应长度:", len(body), "字符\n")
    preview_len = min(800, len(body))
    print("[响应内容预览]")
    print(body[:preview_len])
    if len(body) > preview_len:
        print("\n  ... (已截断，共", len(body), "字符)")
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
