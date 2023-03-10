# 第五章 確認架構特性

#### 確認架構特性

了解關鍵的領域目標與狀況，讓架構師得以將領域考量轉譯成各種能力。

與利益相關方合作定義驅動架構特性時，訣竅是努力讓最後的清單越減短越好。

一個常見的架構反模式是嘗試去設計一個支援所有特性的通用架構。每一種支援的特性都會讓系統的設計複雜化，不要執著於特性數量，而是著重在讓設計簡化。



許多架構師都想依照優先順序處理應用或系統必須支援的最終架構特性清單。大部分情況是徒勞無功，也會和利益相關方產生許多不必要的挫折與意見分歧。更好的作法是讓利益相關方從最後清單中，**挑選三項最重要的特性**，不只容易達成共識，也方便架構師取捨。



#### 領域考量轉譯成架構特性

架構師與利益相關方的語言並不一致，架構師談論可擴充性、互通性、容錯，而利益領域方談的是併購、使用者滿意度、上市時間，容易發生溝通不良的情況。

| 領域考量     | 架構特性                                               |
| ------------ | ------------------------------------------------------ |
| 併購         | 互通性、可擴展性、適應性、延展性                       |
| 上市時間     | 敏捷性、可測試性、可部署性                             |
| 使用者滿意度 | 效能、可用性、容錯、可測試性、可部署性、敏捷性、安全性 |
| 競爭優勢     | 敏捷性、可測試性、可部署性、可用性、容錯               |
| 時程與預算   | 簡化、可用性                                           |



```
例如法規要求一定要準時完成收盤的基金定價，除了注意效能還有其他考量：
1. 系統如果需要時無法使用，則沒人在乎系統多快
2. 隨著領域成長並創造更多基金後，系統必須能夠延展以及即時完成當天的處理工作
3. 系統不只要可用，還必須夠可靠才不會在計算收盤基金定價時當機
4. 如果收盤基金定價計算完成85%時發生當機，必須要能恢復並且從當機時的訂價重啟計算
```



#### 從需求提取架構特性

有些架構特性來自於需求文件的明確須要求。

```
例如架構師處理一個大學生課程註冊的應用，為簡化數字，假設學校有一千名學生，以及十小時的註冊時間。那麼系統的設計規模應該要很一致，假設註冊過程中，學生會均勻分散在整個區間嗎？或者依據大學生的習慣與傾向，系統應該要有辦法在最後十分鐘處理一千個學生的同時註冊？
```



#### 架構套路的起源

預先定義好的幾個段落：

1. 描述： 系統嘗試解決整個領域問題是甚麼
2. 使用者：預期中系統使用者數目即型態
3. 需求：領領域的需求來自於領域的使用者或專家
4. 額外背景：許多架構師蓋考量的事物並未明白表達在需求中，透過問題領域的隱性知識來表達



#### 案例

```
描述：一家全國性的三明治商便想起動線上訂購
使用者；幾千人，一天下來可能達百萬人
需求：
	1. 使用者下訂單，被通知一個取三明治的時間以及地圖導航至店家
	2. 如果提供外送，派遣司機將三明治送給使用者
	3. 可以透過行動裝置使用服務
	4. 提供全國性的每日促銷/特餐
	5. 提供當地的每日促銷/特餐
	6. 接受線上、親自以及外送貨到付款
額外背景：
	1. 每家店舖經營者不同
	2. 公司有擴展到海外計畫
	3. 目標聘請廉價勞力以最大化獲利
```

最早關注的其中一個細節是使用的人數可能達百萬，所以可擴充性是最重要的架構特性之一。問題敘述並未明確要求，而是將該需求表達為一個預期的使用者項目。可能也需要彈性，處理突發請求的能力(突然很多人要買)，潛在的問題也需要能夠被確認。



```
檢視架構特性；
1. 和地圖的整合：外部地圖意味著整合點的存在，影響可靠度。如果呼叫失敗該怎麼辦？或是提供沒有交通資訊的服務
2. 透過行動裝置使用服務：影響應用的設計，必須打造出可攜的網站應用程式，可能會想定義與網頁載入時間和其他特性相關的效能架構，需要和利益方以及專業人士多加討論
3. 每日促銷/特餐：指定了餐點的客製化程度，以及隱含著以地址為基礎的客製化交通資訊，基於這些可以考慮將客製化程度當成一種架構特性。例如透過外掛架構提供客製化支援。
4. 線上、外送貨到付款：隱含著安全性的要求
5. 每間加盟店的經營者不同：此需求可能對架構的費用加以限制，要查核可行性(考慮費用、時間、人員技術等限制)，以了解一個簡單或有所犧牲的架構是否可被接受
6. 擴展到海外的計畫：隱含著國際化
7. 聘請廉價勞力：暗示易用性很重要，但此需求偏向設計而非架構特性
```



#### 隱含特性

許多特性並未指定在需求文件，但卻構成設計的重要面向。通常可用性和可靠性關係密切，而安全性似乎是每個系統的隱含特性。如果安全性影響設計某些結構面向，而且對應用及其關鍵或重要，則架構師會把安全性是為一種特性。

案例中架構師可能假設付款由第三方設計，只要開發人員遵循一般的安全性保全作法，不需要特殊的結構設計來遷就安全性。最後還有客製化行為，架構應該要支援那些客製化的能力，但他對應用程式的成功並沒有那麼關鍵。



#### 設計 vs. 架構與取捨

架構師可能將客製化能力認定為系統一部份，但問題會變成這到底是屬於架構還是設計？架構隱含一些結構上的元件，而設計又存在於架構之內，並從結構上支援客製化，可以選擇像是微核心之類的風格。

考量因素：

1. 有好的理由讓人不選擇實作微核心架構嗎？
2. 其他想要的特性，在某種設計下實現會比其他設計來得困難嗎？
3. 每種設計 vs. 模式底下要支援所有架構特性的成本有多高

不應太強調找到完全正確的架構特性組合，因為開發人員有很多方法可以對功能進行實作，但是正確的辨認重要的結構元素，有助於實現更簡單、優雅的設計。沒有最好的設計，只有別無選擇下的一組考量。

排定優先順序，試著找到最簡單且必要的一組集合，一定確認特性後，一個有用的練習是嘗試去決定最不重要的特性是哪一個，如果要拿掉一項，該拿哪一項？通常會移除掉顯性的特性，因為隱性特性對廣義的成功不可或缺。
