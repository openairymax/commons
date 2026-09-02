# airy_ime 词典数据说明

`airy_ime.dat` 是 airy_ime 内置拼音输入法（全拼）的只读词典，由
`tools/gen_ime_dict.py` 生成。格式见脚本头注释（Header + 拼音条目表 +
候选区 + 字符串池，数据区带 CRC32 校验）。

## 数据来源（均为 MIT 授权的开源数据）

| 数据 | 来源 | 授权 | 用途 |
|------|------|------|------|
| 汉字拼音 | [xiaohk/pinyin_data](https://github.com/xiaohk/pinyin_data)（基于 Unicode Unihan kMandarin/kXHC1983/kHanyuPinyin 等字段） | MIT | 41216 个汉字 → 首选读音 |
| 词组与词频 | [fxsjy/jieba](https://github.com/fxsjy/jieba) `dict.txt` | MIT | 常用词组 + 频次排序 |

## 重新生成

```sh
# 1. 获取原始数据（源码区外，避免污染）
curl -sL -o pinyin.json https://raw.githubusercontent.com/xiaohk/pinyin_data/master/pinyin/pinyin.json
# jieba dict.txt 需从 PyPI 包内获取（pip download jieba 后解压取 jieba/dict.txt）

# 2. 生成词典（默认 5 万词组、词长 ≤6）
python3 ../gen_ime_dict.py \
    --pinyin-json /path/pinyin.json --jieba-dict /path/dict.txt \
    --out data/airy_ime.dat
```

## 数据合规

- 全部本地只读，无网络请求；词典随安装分发，运行零外部依赖。
- 拼音约定：去声调（zhōng→zhong）；ü 以 v 表示（lü→lv）。
- 多音字取首选读音；词组拼音 = 逐字首选音拼接。
- 候选排序：词组按词频降序；单字字频 = 自身频次 + 该字在 top 词组中的
  累计频次（标准字频统计，保证"国/中/我"等常用字居前，符合输入直觉）。
