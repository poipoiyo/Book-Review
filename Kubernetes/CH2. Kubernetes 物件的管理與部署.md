# 第二章 Kubernetes 物件的管理與部署

Kubernetes總共有五種方式，仔細分類又可以將其分成兩大類型，分別是Imperative和Declaration兩種不同走向。

Imperative使用命令式管理，可以使用kubectl指令，快速創建、更新、刪除Kubernetes。Declaration將多個對象配置文件存在一個目錄中，使用kubectl apply根據需求創建和更新kubernetes，不會將改動一併傳回文件內。



一個應用程式如果將其部署到Kubernetes裡面，可探討的議題：

1. 該應用程式是否需要散播給其他使用者使用，其他使用者屬於相同單位還是不同？
2. 該應用程式是否需要版本控制來提供不同版本的需求？
3. 該應用程式是否會需要不同環境而有不同的設定？



#### 應用程式發布

希望有個方便平台來管理這些Kubernetes應用程式，需要一個機制來包裝，要考慮的議題：

1. 文件說明系統：如何讓外部使用者知道如何使用以及注意事項
2. 依賴性系統：如果該應用程式本身又依賴其他應用程式，要如何讓使用者可以順利安裝全部所需資源
3. 安裝系統：可以讓開發者和使用者都方便去上傳和下載這些應用程式，同時也能更輕鬆的安裝、升級、移除



#### 版本控制

不同版本間所使用的參數指令也不同



#### 客製化

基於YAML描述的Kubernetes物件，不同的環境常常會需要不同的設定。應用程式是否有辦法讓使用者很方便的去進行客製化的設定，以及保存客製化以利後續的維護和修改。



#### 解決方案

透過原生YAML來滿足前述問題的解決方式就是搭配Git的版本，將全部YAML物件都寫進單一檔案，透過Github的方式來管理不同版本且提供連結下載。例如：Helm, Kustomize, Jsonnetm Ksonnet等。

使用者可以非常輕鬆的用Helm指令去下載各式各樣已經打包好的Kubernetes應用程式，但Template的語法也為人詬病，可以嘗試用Kustomize, Jsonnet等不同的解決方案。



### 2.1 Helm介紹

用來管理Kubernetes應用程式的解決方案，幫助使用者去定義、安裝、升級等各式各樣的應用程式，可以輕鬆的利用版本控制去發佈，減少複製貼上的行為。



架構概念已Kubernetes YAML檔案為基礎上面，加一層抽象層，Helm的工具會專注於處理這個抽象層，根據情境產生獨一無二的YAML檔案，並將整包檔案安裝到Kubernetes內。



所有應用程式都會稱為Helm Chart，至少包含：

1. Helm Charts名城、版本號、使用方式、注意事項
2. Kubernetes YAML檔案



所使用的YAML檔案長的很像原生YAML語法的變形體，整合基於Golang的Template架構，根據參數產生出不同內容的YAML檔案，安裝到Kubernetes內稱為Release



### 2.2 Helm範例

```yaml
apiVersion: v1
kind: Service
metadata:
	name: example
	labels:
		app: example
spec:
	type: ClusterIP
	ports:
		- port: 80
		  targetPort: http
		  protocol: TCP
		  name: http
	selector:
	app.kubernetes.io/name: example
	app.kubernetes.io/instance: example
```



Helm引入Template概念，解決不同環境下的不同設定的改造

```yaml
apiVersion: v1
kind: Service
metadata:
	name: example
	labels:
		app: example
spec:
	type: {{ .Values.service.type }}
	ports:
		- port: {{ .Values.service.port }}
		  targetPort: http
		  protocol: TCP
		  name: http
	selector:
	app.kubernetes.io/name: example
	app.kubernetes.io/instance: example
```



### 2.3 打造第一個Helm Chart

官方網站：https://helm.sh/docs/intro/install

安裝後可以使用指令創建一個基於Nginx的應用程式

```
$ helm create ithome
tree ithome
```

有五個和Kubernetes有關的物件：deployment.yaml, hpa.yaml, ingress.yaml, service.yaml, serviceaccount.yaml，都採用template的方式來客製化。還有values.yaml，包含各式各樣的變數與預設值。



嘗試將helm chart安裝到目標Kubernetes叢集中，創建測試用namespace→安裝到系統中的ithome namespace，將release命名為helm-test

```
$ kubectl create ns ithome-test
$ helm install -n ithome-test helm-test ithome
```



觀察namespace內的其他資源、觀察系統上安裝了哪些release、觀察release上的各種資料，觀察manifest觀察被渲染後的YAML內容

```
$ kubectl -n ithome-test get all
$ helm -n ithome-test ls
$ helm get --help
$ helm n ithome-test get manifest helm-test
```



得知當前release的客製化內容、升級運行的release並給予不同的設定，用set覆蓋掉values.yaml內容

```
$ helm -n ithome-test get values helm-test
$ helm -n ithome-test upgrade helm-test --set service.type=NodePort ithome
```



#### 散播與發佈

helm的好處是可以輕鬆安裝別人的設計和準備好的helm chart

```
$ helm repo add test https://prometheus-community.github.io/helm-charts
$ helm search repo test
$ helm install test/alertmanager --generate-name
```









