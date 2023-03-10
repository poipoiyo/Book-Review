# 第四章 架構特性

滿足三項準則：

1. 指定與領域無關的設計考量
2. 影響設計的某些結構面向
3. 對應用程式的成功至為成功關鍵或重要



#### 指定與領域無關的設計考量

架構特性在考量如何將需求實現、以及為何做出特定選擇的情形下，指定通向成功的運維與設計準則。

例如：不會有文件陳述「避免技術負債」，但卻架構師和設計人員常有的設計考量。



#### 影響設計的某些結構面向

此架構特性需要特殊的結構考量才能成功嗎？例如專案的安全性都必須有預防措施做底線，但是如果必須做特殊設計時，問題就提升到架構特性的層面了。

```
一個範例系統的兩個付款案例：
1. 第三方付款處理器：如果在某個整合點處理付款細節，架構無須特殊考量，設計本身應加上標準的安全性保全做法。
2. 應用內的付款處理：如果應用程式必須處理付款，架構師可以為此墓的設計特定的模組、元件或服務，從結構上隔離重要的安全性考量。
```



#### 對應用程式的成功至為關鍵或重要

應用程式可能支援很多架構特性，而每種都會增加設計的複雜度，因此架構師的關鍵任務為選擇最少的架構特性。

進一步把架構特性細分成隱含、顯性的特性。隱含的很少出現在需求上，但對專案的成功不可或缺，例如可用性、可靠度、安全性。顯性的則會出現在需求文件上。



#### 運維架構特性

1. 可用性：系統可持續使用的時間有多長
2. 連續性：災害復原能力
3. 效能：包含壓力測試、峰值測試、函數使用頻率分析、需求容量與回應時間，驗收需要好幾個月自行跑過。
4. 可恢復性：災害發生時多快能再上線，影響備份策略與重複硬體
5. 可靠性：評估系統是否安全可靠，或是否及其關鍵到影響人命，如果失效會造成公司龐大金錢損失。
6. 強健性：執行時如果網路連線失效、斷電、硬體錯誤，能夠處理錯誤的能力。
7. 可擴展性：當使用者或請求數目增加時，系統仍能運作的能力。



#### 結構上的架構特性

1. 可配置性：終端使用者輕易更改軟體配置的能力(透過可用的介面)
2. 延伸性：加入新功能的重要性如何
3. 可安裝性：系統安裝在所有平台是否簡易
4. 槓桿能力：在多個產品上使用共同元件的能力
5. 本地化：在進入/查詢畫面支援多語言的資料欄位。至於在報告上面，必須支援多位元組字元的需求，以及測量各種貨幣單位。
6. 可維護性：如果套用變更以強化系統有多容易
7. 可攜性：系統需要在一個以上的平台運作嗎？
8. 可支援性：系統需要哪種層次的技術支援，除錯需要哪種層次的登錄以及其他工具。
9. 可升級性：在伺服器端和客戶端快速升級的能力。



#### 跨領域架構特性

1. 可及性：可觸及所有使用者，像是色盲或聽力受損等失能的人。
2. 可歸檔：資料在一段時間後得歸檔或刪除嗎？
3. 認證：安全性需求，使用者是否是宣稱的人
4. 授權：安全性需求，使用者是否只能存取特定功能
5. 合法：系統運作的法律限制為何
6. 隱私：能夠對內部公司員工隱藏交易
7. 安全性：遭料庫的資料需要加密嗎，或內部系統間的網路通信需要加密嗎，對遠端使用者要加密嗎
8. 可支援性：應用需要哪種層次的技術支援？
9. 易用性/可行性：使用者需要接受甚麼層次的運鏈，才能利用方案完成目標



許多術語既不精確也含糊不清，例如對某些系統，互通性和相容性似乎等價，然而互通性暗指的是與其他系統整合的容易程度，也就是應有公開、文件化的應用程式介面(API)。

另一方面，相容性更關注的是工業與領域標準。另一個例子是易學性，定義使用者學用軟體的容易程度，另一個例子是透過機器學習演算法進行自我配置或自我最佳化。

許多定義有所重疊，例如可用度和可靠度。



#### ISO定義

1. 效能效率：測量已知情況下，相對於資源使用量所展現的效能，包含時間行為(回應時間、產出測量)、資源使用率以及容量

2. 相容性：系統和其他磣品、系統、元件交換資訊的能力。包含共存及互通性。

3. 易用性：使用者能以有效率、滿意的方式使用系統達成目的。包含適當性、可識別性、易學性、使用者錯誤保護、可及性。

4. 可靠度：系統能在特定情況下，運行一段特定時間的程度。包含成熟度、可用性、容錯、可恢復性。

5. 安全性：軟體保護資訊與資料的程度。包含機密性、完整性、不可否認性、可追責性、真實性。

6. 可維護性：代表開發人員能依據環境及需求，針對軟體效果及效率改善的程度。包含模組化、復用性、可分析性、可修改性、可測試性。

7. 可攜性：能將系統轉移到另一個環境的能力。包含適應性、可安裝性、可取代性。

