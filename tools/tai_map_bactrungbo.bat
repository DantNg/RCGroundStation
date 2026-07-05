@echo off
rem Tai truoc tile ban do khu vuc Bac Trung Bo vao cache cua app (~/.lite_gcs/tiles).
rem Dung Python (SSL rieng cua Python) nen KHONG dinh loi OpenSSL cua app Qt.
rem Bam doi (double-click) file nay khi may DANG co internet.
chcp 65001 >nul
cd /d "%~dp0"

echo ================================================================
echo   TAI BAN DO OFFLINE - Bac Trung Bo (Satellite)
echo   (Thanh Hoa - Nghe An - Ha Tinh - Quang Binh - Quang Tri - Hue)
echo ================================================================
echo.

rem Vung Bac Trung Bo: west south east north = 104.0 16.0 108.2 20.8
echo [1/1] Tai lop ve tinh (Satellite) zoom 5-13 cho toan vung...
python prefetch_tiles.py --provider Satellite --bbox 104.0 16.0 108.2 20.8 --zmin 5 --zmax 13 --yes

echo.
echo ================================================================
echo   XONG. Neu dong "Tai moi" > 0 la da co tile.
echo   Mo lai app, chon lop Satellite, keo den vung bay.
echo ================================================================
pause
