# 第一章 DevOps 與 Kubernetes 的生態探討

#### DevOps

**DevOps**（**Dev**elopment和**Op**erations的組合詞）是一種重視「軟體開發人員（Dev）」和「IT運維技術人員（Ops）」之間溝通合作的文化、運動或慣例。透過整體流程的修正，共同語言的搭建以及工具的使用，讓團隊有能利以更頻繁且可靠的方式去更新與測試產品。



不同團隊會依據團隊數量、人數多寡、使用的雲端架構、產品面項等條件，影響最後如何實踐。實作DevOps都會講到CI(Continuous integration) / CD(Contionuous Delivery, Continuous Deployment)，所謂的持續整合與持續交付/持續部署。



透過CI的流程，開發人員能夠針對修改的程式碼進行自動化的測試與建置，能協助更快的發現錯誤並改善品質。透過CD流程，開發人員所修改的，程式碼可以自動部署到生產環境，可以測試環境，也可以是正式面對客戶的環境。CD可以分成持續交付和持續部署，差別在於更新時會不會有人為介入進行手動核准。



### 1.1 Cloud Native

越來越多團隊使用Kubernetes作為容器管理平台，所有的服務都以容器的方式部署於其上。

CNCF(Cloud Native Computing Foundation)是隸屬於Linux Foundation底下的專案，宣布成立時，Kubernetes也同時加入CNCF底下成為至今最知名的專案。致力於培育與維護一個中立的開元生態系統來推廣雲原生相關技術。



### 1.2 CI/CD可以怎麼玩

從開發階段到部署階段中可以探討的環節：

1. Kubernetes內的應用程式該如何包裝？原生YAML還是Helm？
2. 本地開發者需要Kubernetes來測試Kubernetes嗎？
3. CI pipeline系統要選擇哪一套？
4. CI pipeline過程中，需要Kubernetes來測試應用程式嗎？
5. Container Registry要使用雲端服務還是要自架架該如何整合，自架的話該如何使用與整合？
6. CD pipeline要選擇哪套解決方案？要如何觸發？
7. CD pipeline過程中，要怎麼將應用程式更新到遠方的Kubernetes？自架的和服務平台的會有差別嗎？
8. CD更新中，如果有機密資料該怎麼處理？







