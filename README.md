# Điều khiển máy tính từ xa - Mạng máy tính

## Giới thiệu

Một chương trình đơn giản để điều khiển máy tính từ xa thông qua gmail. 

## Công nghệ sử dụng

*   Ngôn ngữ lập trình chính: C++ (C++17), Python (3.11)
*   Hệ thống Build: CMake
*   Thư viện để giao tiếp Email: API gmail
*   Thư viện cho lập trình Socket (TCP/IP): Winsock

## Hướng dẫn Build 
Cài đặt thư viện OpenCV 
https://opencv.org/releases/
vào trong thư mục project (Tại folder `./opencv`)

Build project bằng lệnh
```
.\build_all
```

## Hướng dẫn Cấu hình và Chạy

### Ở phía máy Server
Chạy server bằng lệnh
```
.\run_server
```

### Ở phía máy Client
Cài đặt các package python để chạy script đọc và gửi email

```
pip install --upgrade google-api-python-client google-auth-httplib2 google-auth-oauthlib
```

Chạy client bằng lệnh
```
.\run_client
```

Sau đó nhập IP và Port (mặc định là 8080) của máy server để kết nối với server

## Thông tin Nhóm
*   Huỳnh Thái Hoàng - 24127171
*   Tôn Thất Nhật Minh - 24127083
*   Nguyễn Sơn Hải - 24127163
