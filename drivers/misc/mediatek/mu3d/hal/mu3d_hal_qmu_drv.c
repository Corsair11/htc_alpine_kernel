/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifdef USE_SSUSB_QMU

#include "mu3d_hal_osal.h"
#define _MTK_QMU_DRV_EXT_
#include "mu3d_hal_qmu_drv.h"
#undef _MTK_QMU_DRV_EXT_
#include "mu3d_hal_usb_drv.h"
#include "mu3d_hal_hw.h"

PBD get_bd(USB_DIR dir, DEV_UINT32 num)
{
	PBD ptr;

	if (dir == USB_RX) {
		ptr = (PBD) Rx_bd_List[num].pNext;

		os_printk(K_DEBUG, "%s Rx_bd_List[%d].pNext=%p\n", __func__, num,
			  (Rx_bd_List[num].pNext));

		if ((Rx_bd_List[num].pNext + 1) < Rx_bd_List[num].pEnd)
			Rx_bd_List[num].pNext++;
		else
			Rx_bd_List[num].pNext = Rx_bd_List[num].pStart;

	} else {
		ptr = (PBD) Tx_bd_List[num].pNext;

		os_printk(K_DEBUG, "%s Tx_gpd_List[%d].pNext=%p\n", __func__, num,
			  (Tx_bd_List[num].pNext));

		Tx_bd_List[num].pNext++;
		Tx_bd_List[num].pNext = Tx_bd_List[num].pNext + AT_BD_EXT_LEN;

		if (Tx_bd_List[num].pNext >= Tx_bd_List[num].pEnd)
			Tx_bd_List[num].pNext = Tx_bd_List[num].pStart;
	}
	return ptr;
}

PGPD get_gpd(USB_DIR dir, DEV_UINT32 num)
{
	PGPD ptr;

	if (dir == USB_RX) {
		ptr = Rx_gpd_List[num].pNext;

		

		Rx_gpd_List[num].pNext =
		    Rx_gpd_List[num].pNext + (AT_GPD_EXT_LEN / sizeof(TGPD) + 1);

		
		

		if (Rx_gpd_List[num].pNext >= Rx_gpd_List[num].pEnd)
			Rx_gpd_List[num].pNext = Rx_gpd_List[num].pStart;

	} else {
		ptr = Tx_gpd_List[num].pNext;

		

		Tx_gpd_List[num].pNext =
		    Tx_gpd_List[num].pNext + (AT_GPD_EXT_LEN / sizeof(TGPD) + 1);

		
		

		if (Tx_gpd_List[num].pNext >= Tx_gpd_List[num].pEnd)
			Tx_gpd_List[num].pNext = Tx_gpd_List[num].pStart;
	}
	return ptr;
}

void gpd_ptr_align(USB_DIR dir, DEV_UINT32 num, PGPD ptr)
{
	DEV_UINT32 run_next;

	run_next = true;

	

	while (run_next) {
		if (ptr == get_gpd(dir, num))
			run_next = false;
	}
}

dma_addr_t bd_virt_to_phys(void *vaddr, USB_DIR dir, DEV_UINT32 num)
{
	uintptr_t ptr;

	if (dir == USB_RX)
		ptr = rx_bd_map[num].p_desc_dma;
	else
		ptr = tx_bd_map[num].p_desc_dma;

	os_printk(K_DEBUG, "%s %s[%d]phys=%lx<->virt=%p\n", __func__,
		  ((dir == USB_RX) ? "RX" : "TX"), num, ptr, vaddr);

	return (dma_addr_t) ptr;
}

void *bd_phys_to_virt(void *paddr, USB_DIR dir, DEV_UINT32 num)
{
	void *ptr;

	os_printk(K_DEBUG, "bd_phys_to_virt paddr=%p, num=%d\n", paddr, num);

	if (dir == USB_RX)
		ptr = rx_bd_map[num].p_desc;
	else
		ptr = tx_bd_map[num].p_desc;


	return ptr;
}

dma_addr_t mu3d_hal_gpd_virt_to_phys(void *vaddr, USB_DIR dir, DEV_UINT32 num)
{
	uintptr_t ptr;

	if (dir == USB_RX)
		ptr = rx_gpd_map[num].p_desc_dma + (dma_addr_t) (vaddr - rx_gpd_map[num].p_desc);
	else
		ptr = tx_gpd_map[num].p_desc_dma + (dma_addr_t) (vaddr - tx_gpd_map[num].p_desc);

	os_printk(K_DEBUG, "%s %s[%d]phys=%lx<->virt=%p\n", __func__,
		  ((dir == USB_RX) ? "RX" : "TX"), num, ptr, vaddr);

	return (dma_addr_t) ptr;
}

void *gpd_phys_to_virt(void *paddr, USB_DIR dir, DEV_UINT32 num)
{
	void *ptr;

	

	if (dir == USB_RX) {
		ptr =
		    (void *)((uintptr_t) rx_gpd_map[num].p_desc +
			     (uintptr_t) (paddr - rx_gpd_map[num].p_desc_dma));
	} else {
		ptr =
		    (void *)((uintptr_t) tx_gpd_map[num].p_desc +
			     (uintptr_t) (paddr - tx_gpd_map[num].p_desc_dma));
	}

	return ptr;
}

void init_bd_list(USB_DIR dir, int num, PBD ptr, dma_addr_t io_ptr, DEV_UINT32 size)
{
	if (dir == USB_RX) {
		Rx_bd_List[num].pStart = ptr;
		Rx_bd_List[num].pEnd = (PBD) (ptr + size);
		rx_bd_map[num].p_desc = (void *)ptr;
		rx_bd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Rx_bd_List[num].pNext = ptr;

		os_printk(K_DEBUG, "Rx_bd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			  num, Rx_bd_List[num].pStart, Rx_bd_List[num].pNext, Rx_bd_List[num].pEnd);

		os_printk(K_DEBUG, "rx_bd_map[%d] vir=%p dma=%08llx\n", num,
			  rx_bd_map[num].p_desc, (unsigned long long)rx_bd_map[num].p_desc_dma);

		os_printk(K_DEBUG, "vir=%p dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	} else {
		Tx_bd_List[num].pStart = ptr;
		Tx_bd_List[num].pEnd = (PBD) ((DEV_UINT8 *) (ptr + size) + AT_BD_EXT_LEN * size);
		tx_bd_map[num].p_desc = (void *)ptr;
		tx_bd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Tx_bd_List[num].pNext = (PBD) ((DEV_UINT8 *) ptr + AT_BD_EXT_LEN);

		os_printk(K_DEBUG, "Tx_bd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			  num, Tx_bd_List[num].pStart, Tx_bd_List[num].pNext, Tx_bd_List[num].pEnd);

		os_printk(K_DEBUG, "tx_bd_map[%d] vir=%p dma=%08llx\n", num,
			  tx_bd_map[num].p_desc, (unsigned long long)tx_bd_map[num].p_desc_dma);

		os_printk(K_DEBUG, "vir=%p, dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	}
}


void init_gpd_list(USB_DIR dir, int num, PGPD ptr, dma_addr_t io_ptr, DEV_UINT32 size)
{
	if (dir == USB_RX) {
		Rx_gpd_List[num].pStart = ptr;
		Rx_gpd_List[num].pEnd = (PGPD) ((DEV_UINT8 *) (ptr + size) + AT_GPD_EXT_LEN * size);
		rx_gpd_map[num].p_desc = (void *)ptr;
		rx_gpd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Rx_gpd_List[num].pNext = (PGPD) ((DEV_UINT8 *) ptr + AT_GPD_EXT_LEN);

		qmu_printk(K_INFO, "Rx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			   num, Rx_gpd_List[num].pStart, Rx_gpd_List[num].pNext,
			   Rx_gpd_List[num].pEnd);

		qmu_printk(K_INFO, "rx_gpd_map[%d] vir=%p dma=%08llx\n", num,
			   rx_gpd_map[num].p_desc, (unsigned long long)rx_gpd_map[num].p_desc_dma);

		qmu_printk(K_INFO, "vir=%p, dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	} else {
		Tx_gpd_List[num].pStart = ptr;
		Tx_gpd_List[num].pEnd = (PGPD) ((DEV_UINT8 *) (ptr + size) + AT_GPD_EXT_LEN * size);
		tx_gpd_map[num].p_desc = (void *)ptr;
		tx_gpd_map[num].p_desc_dma = io_ptr;
		ptr++;
		Tx_gpd_List[num].pNext = (PGPD) ((DEV_UINT8 *) ptr + AT_GPD_EXT_LEN);

		qmu_printk(K_INFO, "Tx_gpd_List[%d].pStart=%p, pNext=%p, pEnd=%p\n",
			   num, Tx_gpd_List[num].pStart, Tx_gpd_List[num].pNext,
			   Tx_gpd_List[num].pEnd);

		qmu_printk(K_INFO, "tx_gpd_map[%d] vir=%p dma=%08llx\n", num,
			   tx_gpd_map[num].p_desc, (unsigned long long)tx_gpd_map[num].p_desc_dma);

		qmu_printk(K_INFO, "vir=%p, dma=%08llx\n", ptr, (unsigned long long)io_ptr);
	}
}

void free_gpd(USB_DIR dir, int num)
{
	if (dir == USB_RX) {
		os_memset(Rx_gpd_List[num].pStart, 0,
			  MAX_GPD_NUM * (sizeof(TGPD) + AT_GPD_EXT_LEN));
	} else {
		os_memset(Tx_gpd_List[num].pStart, 0,
			  MAX_GPD_NUM * (sizeof(TGPD) + AT_GPD_EXT_LEN));
	}
}


static dma_addr_t Tx_gpd_ioptr[15];
static dma_addr_t Rx_gpd_ioptr[15];

void _ex_mu3d_hal_free_qmu_mem(struct device *dev)
{
	DEV_UINT32 i;
	DEV_UINT32 size = (sizeof(TGPD) + AT_GPD_EXT_LEN) * MAX_GPD_NUM;

	qmu_printk(K_INFO, "_ex_mu3d_hal_free_qmu_mem +\n");
	for (i = 1; i <= MAX_QMU_EP; i++) {
#if 0
		kfree(Rx_gpd_head[i]);
		kfree(Tx_gpd_head[i]);
#else
		dma_free_coherent(dev, size, Rx_gpd_head[i], Rx_gpd_ioptr[i]);
		dma_free_coherent(dev, size, Tx_gpd_head[i], Tx_gpd_ioptr[i]);
#endif
	}
	qmu_printk(K_INFO, "_ex_mu3d_hal_free_qmu_mem -\n");
}

void _ex_mu3d_hal_alloc_qmu_mem(struct device *dev)
{
	DEV_UINT32 i, size;
	TGPD *ptr;
	dma_addr_t io_ptr;
	dma_addr_t dma_handle;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		
		size = (sizeof(TGPD) + AT_GPD_EXT_LEN) * MAX_GPD_NUM;
		ptr = (TGPD *) dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
		
		if (!ptr) {
			qmu_printk(K_ERR, "Failed to allocate dma buffer for Rx GPD [ep %d]\n", i);
			
		}
		
		memset(ptr, 0, size);
		Rx_gpd_ioptr[i] = io_ptr = dma_handle;

		init_gpd_list(USB_RX, i, ptr, io_ptr, MAX_GPD_NUM);
		Rx_gpd_end[i] = ptr;

		qmu_printk(K_INFO, "ALLOC RX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			   i, Rx_gpd_end[i], (unsigned long long)io_ptr);

		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[i]);
		Rx_gpd_head[i] = Rx_gpd_last[i] = Rx_gpd_end[i];

		qmu_printk(K_INFO, "RQSAR[%d]=%08llx\n", i,
			   (unsigned long long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[i], USB_RX, i));

		
		size = (sizeof(TGPD) + AT_GPD_EXT_LEN) * MAX_GPD_NUM;
		ptr = (TGPD *) dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
		
		if (!ptr) {
			qmu_printk(K_ERR, "Failed to allocate dma buffer for Tx GPD [ep %d]\n", i);
			
		}
		
		memset(ptr, 0, size);
		Tx_gpd_ioptr[i] = io_ptr = dma_handle;

		init_gpd_list(USB_TX, i, ptr, io_ptr, MAX_GPD_NUM);
		Tx_gpd_end[i] = ptr;

		qmu_printk(K_INFO, "ALLOC TX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			   i, Tx_gpd_end[i], (unsigned long long)io_ptr);

		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[i]);
		Tx_gpd_head[i] = Tx_gpd_last[i] = Tx_gpd_end[i];

		qmu_printk(K_INFO, "TQSAR[%d]=%08llx\n", i,
			   (unsigned long long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[i], USB_TX, i));
	}
}

void mu3d_hal_free_qmu_mem(void)
{
	DEV_UINT32 i;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		kfree(Rx_gpd_head[i]);
		kfree(Tx_gpd_head[i]);
		kfree(Rx_bd_List[i].pStart);
		kfree(Tx_bd_List[i].pStart);
	}
}

void mu3d_hal_alloc_qmu_mem(void)
{
	DEV_UINT32 i, size;
	TGPD *ptr;
	dma_addr_t io_ptr;
	TBD *bptr;
	dma_addr_t io_bptr;

	for (i = 1; i <= MAX_QMU_EP; i++) {
		
		size = sizeof(TGPD);
		size *= MAX_GPD_NUM;
		ptr = (TGPD *) os_mem_alloc(size);
		os_memset(ptr, 0, size);

		io_ptr = dma_map_single(NULL, ptr, size, DMA_TO_DEVICE);
		init_gpd_list(USB_RX, i, ptr, io_ptr, MAX_GPD_NUM);
		Rx_gpd_end[i] = ptr;

		os_printk(K_DEBUG, "ALLOC RX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			  i, Rx_gpd_end[i], (unsigned long long)io_ptr);

		
		TGPD_CLR_FLAGS_HWO(Rx_gpd_end[i]);
		Rx_gpd_head[i] = Rx_gpd_last[i] = Rx_gpd_end[i];

		os_printk(K_DEBUG, "RQSAR[%d]=%08llx\n", i,
			  (unsigned long long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[i], USB_RX, i));

		
		size = sizeof(TGPD);
		size += AT_GPD_EXT_LEN;
		size *= MAX_GPD_NUM;
		ptr = (TGPD *) os_mem_alloc(size);
		os_memset(ptr, 0, size);

		io_ptr = dma_map_single(NULL, ptr, size, DMA_TO_DEVICE);
		init_gpd_list(USB_TX, i, ptr, io_ptr, MAX_GPD_NUM);
		Tx_gpd_end[i] = ptr;

		os_printk(K_DEBUG, "ALLOC TX GPD End [%d] Virtual Mem=%p, DMA addr=%08llx\n",
			  i, Tx_gpd_end[i], (unsigned long long)io_ptr);

		TGPD_CLR_FLAGS_HWO(Tx_gpd_end[i]);
		Tx_gpd_head[i] = Tx_gpd_last[i] = Tx_gpd_end[i];

		os_printk(K_DEBUG, "TQSAR[%d]=%08llx\n", i,
			  (unsigned long long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[i], USB_TX, i));

		
		size = (sizeof(TBD));
		size *= MAX_BD_NUM;
		bptr = (TBD *) os_mem_alloc(size);
		os_memset(bptr, 0, size);
		io_bptr = dma_map_single(NULL, bptr, size, DMA_TO_DEVICE);
		init_bd_list(USB_RX, i, bptr, io_bptr, MAX_BD_NUM);

		
		size = (sizeof(TBD));
		size += AT_BD_EXT_LEN;
		size *= MAX_BD_NUM;
		bptr = (TBD *) os_mem_alloc(size);
		os_memset(bptr, 0, size);
		io_bptr = dma_map_single(NULL, bptr, size, DMA_TO_DEVICE);
		init_bd_list(USB_TX, i, bptr, io_bptr, MAX_BD_NUM);
	}
}

void _ex_mu3d_hal_init_qmu(void)
{
	DEV_UINT32 i;
	DEV_UINT32 QCR = 0;

	
	for (i = 1; i <= MAX_QMU_EP; i++) {
		qmu_printk(K_INFO, "==EP[%d]==Start addr RXQ=0x%08lx, TXQ=0x%08lx\n", i,
			   (uintptr_t) mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i),
			   (uintptr_t) mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		QCR |= QMU_RX_EN(i);
		QCR |= QMU_TX_EN(i);
		os_writel(USB_QMU_RQSAR(i), mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i));
		os_writel(USB_QMU_TQSAR(i), mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		gpd_ptr_align(USB_TX, i, Tx_gpd_end[i]);
		gpd_ptr_align(USB_RX, i, Rx_gpd_end[i]);
	}

	
	os_writel(U3D_QIESR1, TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR |
		  RXQ_EMPTY_IESR | RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
	os_writel(U3D_EPIESR, EP0ISR);
}


void mu3d_hal_init_qmu(void)
{
	DEV_UINT32 i;
	DEV_UINT32 QCR = 0;

	
	for (i = 1; i <= MAX_QMU_EP; i++) {
		os_printk(K_DEBUG, "==EP[%d]==Start addr RXQ=0x%08lx, TXQ=0x%08lx\n", i,
			  (uintptr_t) mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i),
			  (uintptr_t) mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		QCR |= QMU_RX_EN(i);
		QCR |= QMU_TX_EN(i);
		os_writel(USB_QMU_RQSAR(i), mu3d_hal_gpd_virt_to_phys(Rx_gpd_head[i], USB_RX, i));
		os_writel(USB_QMU_TQSAR(i), mu3d_hal_gpd_virt_to_phys(Tx_gpd_head[i], USB_TX, i));
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		gpd_ptr_align(USB_TX, i, Tx_gpd_end[i]);
		gpd_ptr_align(USB_RX, i, Rx_gpd_end[i]);
	}
	
	os_writel(U3D_QGCSR, QCR);
	os_writel(U3D_QIESR0, QCR);
	
	os_writel(U3D_QIESR1,
		  TXQ_EMPTY_IESR | TXQ_CSERR_IESR | TXQ_LENERR_IESR | RXQ_EMPTY_IESR |
		  RXQ_CSERR_IESR | RXQ_LENERR_IESR | RXQ_ZLPERR_IESR);
	os_writel(U3D_EPIESR, EP0ISR);
}

noinline DEV_UINT8 mu3d_hal_cal_checksum(DEV_UINT8 *data, DEV_INT32 len)
{
	DEV_UINT8 *uDataPtr, ckSum;
	DEV_INT32 i;

	*(data + 1) = 0x0;
	uDataPtr = data;
	ckSum = 0;
	
	
	
	mb();
	for (i = 0; i < len; i++)
		ckSum += *(uDataPtr + i);

	return 0xFF - ckSum;
}

void mu3d_hal_resume_qmu(DEV_INT32 q_num, USB_DIR dir)
{
	if (dir == USB_TX) {
		os_writel(USB_QMU_TQCSR(q_num), QMU_Q_RESUME);
		if (!os_readl(USB_QMU_TQCSR(q_num))) {
			qmu_printk(K_WARNIN, "[ERROR]" "%s TQCSR[%d]=%x\n", __func__, q_num,
				   os_readl(USB_QMU_TQCSR(q_num)));
			os_writel(USB_QMU_TQCSR(q_num), QMU_Q_RESUME);
			qmu_printk(K_WARNIN, "[ERROR]" "%s TQCSR[%d]=%x\n", __func__, q_num,
				   os_readl(USB_QMU_TQCSR(q_num)));
		}
	} else if (dir == USB_RX) {
		os_writel(USB_QMU_RQCSR(q_num), QMU_Q_RESUME);
		if (!os_readl(USB_QMU_RQCSR(q_num))) {
			qmu_printk(K_WARNIN, "[ERROR]" "%s RQCSR[%d]=%x\n", __func__, q_num,
				   os_readl(USB_QMU_RQCSR(q_num)));
			os_writel(USB_QMU_RQCSR(q_num), QMU_Q_RESUME);
			qmu_printk(K_WARNIN, "[ERROR]" "%s RQCSR[%d]=%x\n", __func__, q_num,
				   os_readl(USB_QMU_RQCSR(q_num)));
		}
	} else {
		qmu_printk(K_ERR, "%s wrong direction!!!\n", __func__);
		BUG_ON(1);
	}
}

TGPD *_ex_mu3d_hal_prepare_tx_gpd(TGPD *gpd, dma_addr_t pBuf, DEV_UINT32 data_len,
				  DEV_UINT8 ep_num, DEV_UINT8 _is_bdp, DEV_UINT8 isHWO,
				  DEV_UINT8 ioc, DEV_UINT8 bps, DEV_UINT8 zlp)
{
	qmu_printk(K_DEBUG,
		   "[TX]" "%s gpd=%p, epnum=%d, len=%d, zlp=%d, size(TGPD)=%lld, pBuf=%08lx\n",
		   __func__, gpd, ep_num, data_len, zlp, (u64) sizeof(TGPD), (unsigned long)pBuf);

	
	TGPD_SET_DATA(gpd, (unsigned long)pBuf);
	
	TGPD_CLR_FORMAT_BDP(gpd);
	TGPD_SET_BUF_LEN(gpd, data_len);

	
	TGPD_SET_EXT_LEN(gpd, 0);

	if (zlp)
		TGPD_SET_FORMAT_ZLP(gpd);
	else
		TGPD_CLR_FORMAT_ZLP(gpd);

	
	TGPD_CLR_FORMAT_BPS(gpd);

	
	TGPD_SET_FORMAT_IOC(gpd);

	
	Tx_gpd_end[ep_num] = get_gpd(USB_TX, ep_num);
	qmu_printk(K_DEBUG, "[TX]" "Tx_gpd_end[%d]=%p\n", ep_num, Tx_gpd_end[ep_num]);

	
	memset(Tx_gpd_end[ep_num], 0, sizeof(TGPD) + AT_GPD_EXT_LEN);

	
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);

	
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[ep_num], USB_TX, ep_num));

	
	TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	
	TGPD_SET_FLAGS_HWO(gpd);	

	return gpd;
}

TGPD *mu3d_hal_prepare_tx_gpd(TGPD *gpd, dma_addr_t pBuf, DEV_UINT32 data_len,
			      DEV_UINT8 ep_num, DEV_UINT8 _is_bdp, DEV_UINT8 isHWO,
			      DEV_UINT8 ioc, DEV_UINT8 bps, DEV_UINT8 zlp)
{
	DEV_UINT32 offset;
	DEV_INT32 i;
	DEV_INT32 bd_num;
	DEV_UINT32 length;

	TBD *bd_next;
	TBD *bd_head;
	TBD *bd;
	DEV_UINT8 *pBuffer;

	
	
	
	

	os_printk(K_INFO, "%s gpd=%p, epnum=%d, len=%d, _is_bdp=%d\n", __func__,
		  gpd, ep_num, data_len, _is_bdp);

	if (!_is_bdp) {
		
		TGPD_SET_DATA(gpd, (unsigned long)pBuf);
		
		TGPD_CLR_FORMAT_BDP(gpd);
	} else {
		
		bd_head = (TBD *) get_bd(USB_TX, ep_num);
		os_printk(K_INFO, "bd_head=x%p\n", bd_head);

		bd = bd_head;
		os_memset(bd, 0, sizeof(TBD));

		
		length = data_len;

		
		pBuffer = (DEV_UINT8 *) (uintptr_t) (pBuf);

		
		offset = BD_BUF_SIZE;

		
		bd_num = (!(length % offset)) ? (length / offset) : ((length / offset) + 1);

		os_printk(K_INFO, "bd_num=%d\n", bd_num);

		
		if (offset > length)
			offset = length;

		
		for (i = 0; i < bd_num; i++) {
			os_printk(K_INFO, "bd[%d]=%p\n", i, bd);
			if (i == (bd_num - 1)) {	
				TBD_SET_EXT_LEN(bd, 0);	
				TBD_SET_BUF_LEN(bd, length);	
				
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_SET_FLAGS_EOL(bd);	
				TBD_SET_NEXT(bd, 0);	
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	

				
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(TBD), DMA_BIDIRECTIONAL);

				
				
				data_len = 0;

				
				length = 0;
			} else {
				TBD_SET_EXT_LEN(bd, 0);	
				TBD_SET_BUF_LEN(bd, offset);	
				
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_CLR_FLAGS_EOL(bd);	
				
				bd_next = (TBD *) get_bd(USB_TX, ep_num);
				os_memset(bd_next, 0, sizeof(TBD));

				
				TBD_SET_NEXT(bd,
					     (unsigned long)bd_virt_to_phys(bd_next, USB_TX,
									    ep_num));
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	

				
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(TBD), DMA_BIDIRECTIONAL);

				
				length -= offset;

				
				pBuffer += offset;

				
				bd = bd_next;
			}
		}

		
		TGPD_SET_DATA(gpd, (unsigned long)bd_virt_to_phys(bd_head, USB_TX, ep_num));

		
		TGPD_SET_FORMAT_BDP(gpd);
	}

	os_printk(K_INFO, "%s GPD data_length=%d\n", __func__, data_len);

	TGPD_SET_BUF_LEN(gpd, data_len);

	
	TGPD_SET_EXT_LEN(gpd, 0);

	
	if (zlp)
		TGPD_SET_FORMAT_ZLP(gpd);
	else
		TGPD_CLR_FORMAT_ZLP(gpd);

	
	if (bps)
		TGPD_SET_FORMAT_BPS(gpd);
	else
		TGPD_CLR_FORMAT_BPS(gpd);

	
	if (ioc)
		TGPD_SET_FORMAT_IOC(gpd);
	else
		TGPD_CLR_FORMAT_IOC(gpd);

	
	Tx_gpd_end[ep_num] = get_gpd(USB_TX, ep_num);
	os_printk(K_INFO, "Tx_gpd_end[%d]=%p\n", ep_num, Tx_gpd_end[ep_num]);

	
	os_memset(Tx_gpd_end[ep_num], 0, sizeof(TGPD));

	
	TGPD_CLR_FLAGS_HWO(Tx_gpd_end[ep_num]);

	
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Tx_gpd_end[ep_num], USB_TX, ep_num));

	
	if (isHWO) {
		TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	
		TGPD_SET_FLAGS_HWO(gpd);	
	} else {
		TGPD_CLR_FLAGS_HWO(gpd);
		TGPD_SET_CHKSUM_HWO(gpd, CHECKSUM_LENGTH);
	}

	
	dma_sync_single_for_device(NULL, mu3d_hal_gpd_virt_to_phys(gpd, USB_TX, ep_num),
				   sizeof(TGPD), DMA_BIDIRECTIONAL);

#if defined(USB_RISC_CACHE_ENABLED)
	os_flushinvalidateDcache();
#endif

	return gpd;
}

static inline int check_next_gpd(TGPD *gpd, TGPD *next_gpd)
{
	if (((uintptr_t) next_gpd - (uintptr_t) gpd) == 0x40)
		return 1;
	else if (((uintptr_t) gpd - (uintptr_t) next_gpd) == 0x7c0)
		return 1;

	
	qmu_printk(K_ERR, "[RX]" "%p <-> %p\n", gpd, next_gpd);
	return 0;
}

TGPD *_ex_mu3d_hal_prepare_rx_gpd(TGPD *gpd, dma_addr_t pBuf, DEV_UINT32 data_len,
				  DEV_UINT8 ep_num, DEV_UINT8 _is_bdp, DEV_UINT8 isHWO,
				  DEV_UINT8 ioc, DEV_UINT8 bps, DEV_UINT32 cMaxPacketSize)
{
	qmu_printk(K_DEBUG, "[RX]" "%s gpd=%p, epnum=%d, len=%d, pBuf=%08lx\n", __func__,
		   gpd, ep_num, data_len, (unsigned long)pBuf);

	
	TGPD_SET_DATA(gpd, (unsigned long)pBuf);
	
	TGPD_CLR_FORMAT_BDP(gpd);
	TGPD_SET_DataBUF_LEN(gpd, data_len);

	
	TGPD_SET_BUF_LEN(gpd, 0);

	
	TGPD_CLR_FORMAT_BPS(gpd);

	
	TGPD_SET_FORMAT_IOC(gpd);

	
	Rx_gpd_end[ep_num] = get_gpd(USB_RX, ep_num);
	qmu_printk(K_DEBUG, "[RX]" "Rx_gpd_end[%d]=%p gpd=%p\n", ep_num, Rx_gpd_end[ep_num], gpd);

	

	
	memset(Rx_gpd_end[ep_num], 0, sizeof(TGPD) + AT_GPD_EXT_LEN);

	
	TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);

	
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[ep_num], USB_RX, ep_num));

	
	TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	
	TGPD_SET_FLAGS_HWO(gpd);	

	
	
	
	
	
	

	return gpd;
}

TGPD *mu3d_hal_prepare_rx_gpd(TGPD *gpd, dma_addr_t pBuf, DEV_UINT32 data_len,
			      DEV_UINT8 ep_num, DEV_UINT8 _is_bdp, DEV_UINT8 isHWO,
			      DEV_UINT8 ioc, DEV_UINT8 bps, DEV_UINT32 cMaxPacketSize)
{
	DEV_UINT32 offset;
	DEV_INT32 i;
	DEV_INT32 bd_num;
	DEV_UINT32 length;

	TBD *bd_next;
	TBD *bd_head;
	TBD *bd;
	DEV_UINT8 *pBuffer;

	
	if (data_len < GPD_BUF_SIZE)
		_is_bdp = 0;

	os_printk(K_INFO, "%s gpd=%p, epnum=%d, len=%d, _is_bdp=%d, maxp=%d\n", __func__,
		  gpd, ep_num, data_len, _is_bdp, cMaxPacketSize);

	if (!_is_bdp) {
		
		TGPD_SET_DATA(gpd, (unsigned long)pBuf);
		
		TGPD_CLR_FORMAT_BDP(gpd);
	} else {
		
		bd_head = (TBD *) get_bd(USB_RX, ep_num);
		os_printk(K_INFO, "bd_head=x%p\n", bd_head);

		bd = bd_head;
		os_memset(bd, 0, sizeof(TBD));

		
		length = data_len;

		
		pBuffer = (DEV_UINT8 *) (uintptr_t) (pBuf);

		
		offset = BD_BUF_SIZE;

		
		bd_num = (!(length % offset)) ? (length / offset) : ((length / offset) + 1);

		os_printk(K_INFO, "%s bd_num=%d\n", __func__, bd_num);

		
		for (i = 0; i < bd_num; i++) {
			os_printk(K_INFO, "%s bd[%d]=%p\n", __func__, i, bd);
			if (i == (bd_num - 1)) {
				TBD_SET_BUF_LEN(bd, 0);	

				
				length =
				    (!(length % cMaxPacketSize)) ? (length) : ((length /
										cMaxPacketSize) +
									       1) * cMaxPacketSize;
				TBD_SET_DataBUF_LEN(bd, length);
				
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_SET_FLAGS_EOL(bd);	
				TBD_SET_NEXT(bd, 0);	
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	

				
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(TBD), DMA_BIDIRECTIONAL);

				break;
			}
			
			
			{
				TBD_SET_BUF_LEN(bd, 0);	

				
				TBD_SET_DataBUF_LEN(bd, offset);
				
				TBD_SET_DATA(bd, (unsigned long)pBuffer);

				TBD_CLR_FLAGS_EOL(bd);	
				
				bd_next = (TBD *) get_bd(USB_RX, ep_num);
				os_memset(bd_next, 0, sizeof(TBD));

				
				TBD_SET_NEXT(bd,
					     (unsigned long)bd_virt_to_phys(bd_next, USB_RX,
									    ep_num));
				TBD_SET_CHKSUM(bd, CHECKSUM_LENGTH);	

				
				dma_sync_single_for_device(NULL,
							   bd_virt_to_phys(bd, USB_RX, ep_num),
							   sizeof(TBD), DMA_BIDIRECTIONAL);

				
				length -= offset;

				
				pBuffer += offset;

				
				bd = bd_next;
			}
		}

		
		TGPD_SET_DATA(gpd, (unsigned long)bd_virt_to_phys(bd_head, USB_RX, ep_num));

		
		TGPD_SET_FORMAT_BDP(gpd);
	}

	os_printk(K_INFO, "%s GPD data_length=%d\n", __func__, data_len);

	TGPD_SET_DataBUF_LEN(gpd, data_len);
	

	
	TGPD_SET_BUF_LEN(gpd, 0);

	
	if (bps)
		TGPD_SET_FORMAT_BPS(gpd);
	else
		TGPD_CLR_FORMAT_BPS(gpd);

	
	if (ioc)
		TGPD_SET_FORMAT_IOC(gpd);
	else
		TGPD_CLR_FORMAT_IOC(gpd);

	
	Rx_gpd_end[ep_num] = get_gpd(USB_RX, ep_num);
	os_printk(K_INFO, "%s Rx_gpd_end[%d]=%p\n", __func__, ep_num, Tx_gpd_end[ep_num]);

	
	os_memset(Rx_gpd_end[ep_num], 0, sizeof(TGPD));

	
	TGPD_CLR_FLAGS_HWO(Rx_gpd_end[ep_num]);

	
	TGPD_SET_NEXT(gpd,
		      (unsigned long)mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[ep_num], USB_RX, ep_num));

	
	if (isHWO) {
		TGPD_SET_CHKSUM(gpd, CHECKSUM_LENGTH);	
		TGPD_SET_FLAGS_HWO(gpd);	
	} else {
		TGPD_CLR_FLAGS_HWO(gpd);
		TGPD_SET_CHKSUM_HWO(gpd, CHECKSUM_LENGTH);
	}

	
	
	
	

	
	dma_sync_single_for_device(NULL, mu3d_hal_gpd_virt_to_phys(gpd, USB_RX, ep_num),
				   sizeof(TGPD), DMA_BIDIRECTIONAL);

	return gpd;
}

void _ex_mu3d_hal_insert_transfer_gpd(DEV_INT32 ep_num, USB_DIR dir, dma_addr_t buf,
				      DEV_UINT32 count, DEV_UINT8 isHWO, DEV_UINT8 ioc,
				      DEV_UINT8 bps, DEV_UINT8 zlp, DEV_UINT32 maxp)
{
	TGPD *gpd;

	if (dir == USB_TX) {
		gpd = Tx_gpd_end[ep_num];
		_ex_mu3d_hal_prepare_tx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, zlp);
	} else if (dir == USB_RX) {
		gpd = Rx_gpd_end[ep_num];
		_ex_mu3d_hal_prepare_rx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, maxp);
	}
}

void mu3d_hal_insert_transfer_gpd(DEV_INT32 ep_num, USB_DIR dir, dma_addr_t buf,
				  DEV_UINT32 count, DEV_UINT8 isHWO, DEV_UINT8 ioc,
				  DEV_UINT8 bps, DEV_UINT8 zlp, DEV_UINT32 maxp)
{
	TGPD *gpd;

	if (dir == USB_TX) {
		gpd = Tx_gpd_end[ep_num];
		
		mu3d_hal_prepare_tx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, zlp);
	} else if (dir == USB_RX) {
		gpd = Rx_gpd_end[ep_num];
		
		mu3d_hal_prepare_rx_gpd(gpd, buf, count, ep_num, IS_BDP, isHWO, ioc, bps, maxp);
	}
}

void mu3d_hal_start_qmu(DEV_INT32 Q_num, USB_DIR dir)
{
	DEV_UINT32 QCR;
	DEV_UINT32 txcsr;

	if (dir == USB_TX) {
		txcsr = USB_ReadCsr32(U3D_TX1CSR0, Q_num) & 0xFFFEFFFF;
		USB_WriteCsr32(U3D_TX1CSR0, Q_num, txcsr | TX_DMAREQEN);
		QCR = os_readl(U3D_QCR0);
		os_writel(U3D_QCR0, QCR | QMU_TX_CS_EN(Q_num));
#if (TXZLP == HW_MODE)
		QCR = os_readl(U3D_QCR1);
		os_writel(U3D_QCR1, QCR & ~QMU_TX_ZLP(Q_num));
		QCR = os_readl(U3D_QCR2);
		os_writel(U3D_QCR2, QCR | QMU_TX_ZLP(Q_num));
#elif (TXZLP == GPD_MODE)
		QCR = os_readl(U3D_QCR1);
		os_writel(U3D_QCR1, QCR | QMU_TX_ZLP(Q_num));
#endif
		os_writel(U3D_QEMIESR, os_readl(U3D_QEMIESR) | QMU_TX_EMPTY(Q_num));
		os_writel(U3D_TQERRIESR0, QMU_TX_LEN_ERR(Q_num) | QMU_TX_CS_ERR(Q_num));

		qmu_printk(K_DEBUG, "USB_QMU_TQCSR:0x%08X\n", os_readl(USB_QMU_TQCSR(Q_num)));

		if (os_readl(USB_QMU_TQCSR(Q_num)) & QMU_Q_ACTIVE) {
			qmu_printk(K_INFO, "Tx %d Active Now!\n", Q_num);
			return;
		}

		os_writel(USB_QMU_TQCSR(Q_num), QMU_Q_START);

		qmu_printk(K_DEBUG, "USB_QMU_TQCSR:0x%08X\n", os_readl(USB_QMU_TQCSR(Q_num)));
	} else if (dir == USB_RX) {
		USB_WriteCsr32(U3D_RX1CSR0, Q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, Q_num) | (RX_DMAREQEN));
		QCR = os_readl(U3D_QCR0);
		os_writel(U3D_QCR0, QCR | QMU_RX_CS_EN(Q_num));

#ifdef CFG_RX_ZLP_EN
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR | QMU_RX_ZLP(Q_num));
#else
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR & ~(QMU_RX_ZLP(Q_num)));
#endif

#ifdef CFG_RX_COZ_EN
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR | QMU_RX_COZ(Q_num));
#else
		QCR = os_readl(U3D_QCR3);
		os_writel(U3D_QCR3, QCR & ~(QMU_RX_COZ(Q_num)));
#endif

		os_writel(U3D_QEMIESR, os_readl(U3D_QEMIESR) | QMU_RX_EMPTY(Q_num));
		os_writel(U3D_RQERRIESR0, QMU_RX_LEN_ERR(Q_num) | QMU_RX_CS_ERR(Q_num));
		os_writel(U3D_RQERRIESR1, QMU_RX_EP_ERR(Q_num) | QMU_RX_ZLP_ERR(Q_num));

		qmu_printk(K_DEBUG, "USB_QMU_RQCSR:0x%08X\n", os_readl(USB_QMU_RQCSR(Q_num)));

		if (os_readl(USB_QMU_RQCSR(Q_num)) & QMU_Q_ACTIVE) {
			qmu_printk(K_INFO, "Rx %d Active Now!\n", Q_num);
			return;
		}

		os_writel(USB_QMU_RQCSR(Q_num), QMU_Q_START);

		qmu_printk(K_DEBUG, "USB_QMU_RQCSR:0x%08X\n", os_readl(USB_QMU_RQCSR(Q_num)));
	}
#if (CHECKSUM_TYPE == CS_16B)
	os_writel(U3D_QCR0, os_readl(U3D_QCR0) | CS16B_EN);
#else
	os_writel(U3D_QCR0, os_readl(U3D_QCR0) & ~CS16B_EN);
#endif
}

void mu3d_hal_stop_qmu(DEV_INT32 q_num, USB_DIR dir)
{
	if (dir == USB_TX) {
		if (!(os_readl(USB_QMU_TQCSR(q_num)) & (QMU_Q_ACTIVE))) {
			qmu_printk(K_INFO, "Tx%d inActive Now!\n", q_num);
			return;
		}
		os_writel(USB_QMU_TQCSR(q_num), QMU_Q_STOP);
		mb();
		if (wait_for_value(USB_QMU_TQCSR(q_num), QMU_Q_ACTIVE, 0, 10, 100) == RET_SUCCESS)
			qmu_printk(K_INFO, "Tx%d stop Now! CSR=0x%x\n", q_num,
				   os_readl(USB_QMU_TQCSR(q_num)));
		else {
			qmu_printk(K_CRIT, "Tx%d UNSTOPABLE!! CSR=0x%x\n", q_num,
				   os_readl(USB_QMU_TQCSR(q_num)));
			WARN_ON(1);
		}
	} else if (dir == USB_RX) {
		if (!(os_readl(USB_QMU_RQCSR(q_num)) & QMU_Q_ACTIVE)) {
			qmu_printk(K_INFO, "Rx%d inActive Now!\n", q_num);
			return;
		}
		os_writel(USB_QMU_RQCSR(q_num), QMU_Q_STOP);
		mb();
		if (wait_for_value(USB_QMU_RQCSR(q_num), QMU_Q_ACTIVE, 0, 10, 100) == RET_SUCCESS)
			qmu_printk(K_INFO, "Rx%d stop Now! CSR=0x%x\n", q_num,
				   os_readl(USB_QMU_RQCSR(q_num)));
		else {
			qmu_printk(K_CRIT, "Rx%d UNSTOPABLE!! CSR=0x%x\n", q_num,
				   os_readl(USB_QMU_RQCSR(q_num)));
			WARN_ON(1);
		}
	}
}

void mu3d_hal_send_stall(DEV_INT32 q_num, USB_DIR dir)
{
	if (dir == USB_TX) {
		USB_WriteCsr32(U3D_TX1CSR0, q_num,
			       USB_ReadCsr32(U3D_TX1CSR0, q_num) | TX_SENDSTALL);
		while (!(USB_ReadCsr32(U3D_TX1CSR0, q_num) & TX_SENTSTALL))
			;
		USB_WriteCsr32(U3D_TX1CSR0, q_num,
			       USB_ReadCsr32(U3D_TX1CSR0, q_num) | TX_SENTSTALL);
		USB_WriteCsr32(U3D_TX1CSR0, q_num,
			       USB_ReadCsr32(U3D_TX1CSR0, q_num) & ~TX_SENDSTALL);
	} else if (dir == USB_RX) {
		USB_WriteCsr32(U3D_RX1CSR0, q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, q_num) | RX_SENDSTALL);
		while (!(USB_ReadCsr32(U3D_RX1CSR0, q_num) & RX_SENTSTALL))
			;
		USB_WriteCsr32(U3D_RX1CSR0, q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, q_num) | RX_SENTSTALL);
		USB_WriteCsr32(U3D_RX1CSR0, q_num,
			       USB_ReadCsr32(U3D_RX1CSR0, q_num) & ~RX_SENDSTALL);
	}

	os_printk(K_CRIT, "%s %s-EP[%d] sent stall\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		  q_num);
}

void mu3d_hal_restart_qmu(DEV_INT32 q_num, USB_DIR dir)
{
	DEV_UINT32 ep_rst;

	qmu_printk(K_CRIT, "%s : Reset %s-EP[%d]\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		   q_num);

	if (dir == USB_TX) {
		ep_rst = BIT16 << q_num;
		os_writel(U3D_EP_RST, ep_rst);
		os_ms_delay(1);
		os_writel(U3D_EP_RST, 0);
	} else {
		ep_rst = 1 << q_num;
		os_writel(U3D_EP_RST, ep_rst);
		os_ms_delay(1);
		os_writel(U3D_EP_RST, 0);
	}
	mu3d_hal_start_qmu(q_num, dir);
}

void _ex_mu3d_hal_flush_qmu(DEV_INT32 Q_num, USB_DIR dir)
{
	qmu_printk(K_INFO, "%s flush QMU %s-EP[%d]\n", __func__, ((dir == USB_TX) ? "TX" : "RX"),
		   Q_num);

	if (dir == USB_TX) {
		
		mu3d_hal_stop_qmu(Q_num, USB_TX);

		
		Tx_gpd_end[Q_num] = Tx_gpd_last[Q_num] = Tx_gpd_head[Q_num];
		gpd_ptr_align(dir, Q_num, Tx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		
		os_writel(USB_QMU_TQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Tx_gpd_last[Q_num], USB_TX, Q_num));
		qmu_printk(K_INFO, "USB_QMU_TQSAR %x\n", os_readl(USB_QMU_TQSAR(Q_num)));
	} else if (dir == USB_RX) {
		
		mu3d_hal_stop_qmu(Q_num, USB_RX);

		
		Rx_gpd_end[Q_num] = Rx_gpd_last[Q_num] = Rx_gpd_head[Q_num];
		gpd_ptr_align(dir, Q_num, Rx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		
		os_writel(USB_QMU_RQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[Q_num], USB_RX, Q_num));
		qmu_printk(K_INFO, "USB_QMU_RQSAR %x\n", os_readl(USB_QMU_RQSAR(Q_num)));
	}
}

void mu3d_hal_flush_qmu(DEV_INT32 Q_num, USB_DIR dir)
{
	TGPD *gpd_current;

	struct USB_REQ *req = mu3d_hal_get_req(Q_num, dir);

	os_printk(K_INFO, "%s flush QMU %s\n", __func__, ((dir == USB_TX) ? "TX" : "RX"));

	if (dir == USB_TX) {
		
		mu3d_hal_stop_qmu(Q_num, USB_TX);

		
		
		gpd_current = (TGPD *) (uintptr_t) (os_readl(USB_QMU_TQCPR(Q_num)));

		
		if (!gpd_current) {
			
			
			gpd_current = (TGPD *) (uintptr_t) (os_readl(USB_QMU_TQSAR(Q_num)));
		}

		
		os_printk(K_INFO, "gpd_current(P) %p\n", gpd_current);
		gpd_current = gpd_phys_to_virt(gpd_current, USB_TX, Q_num);
		os_printk(K_INFO, "gpd_current(V) %p\n", gpd_current);

		
		Tx_gpd_end[Q_num] = Tx_gpd_last[Q_num] = gpd_current;
		gpd_ptr_align(dir, Q_num, Tx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		
		os_writel(USB_QMU_TQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Tx_gpd_last[Q_num], USB_TX, Q_num));
		os_printk(K_INFO, "USB_QMU_TQSAR %x\n", os_readl(USB_QMU_TQSAR(Q_num)));
		req->complete = true;
		
	} else if (dir == USB_RX) {
		
		mu3d_hal_stop_qmu(Q_num, USB_RX);

		
		
		gpd_current = (TGPD *) (uintptr_t) (os_readl(USB_QMU_RQCPR(Q_num)));
		if (!gpd_current) {
			
			
			gpd_current = (TGPD *) (uintptr_t) (os_readl(USB_QMU_RQSAR(Q_num)));
		}

		
		os_printk(K_INFO, "gpd_current(P) %p\n", gpd_current);
		gpd_current = gpd_phys_to_virt(gpd_current, USB_RX, Q_num);
		os_printk(K_INFO, "gpd_current(V) %p\n", gpd_current);

		
		Rx_gpd_end[Q_num] = Rx_gpd_last[Q_num] = gpd_current;
		gpd_ptr_align(dir, Q_num, Rx_gpd_end[Q_num]);
		free_gpd(dir, Q_num);

		
		os_writel(USB_QMU_RQSAR(Q_num),
			  mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[Q_num], USB_RX, Q_num));
		os_printk(K_INFO, "USB_QMU_RQSAR %x\n", os_readl(USB_QMU_RQSAR(Q_num)));
		req->complete = true;
		
	}
}

void mu3d_reset_gpd_resource(void)
{
	DEV_UINT32 i;
	os_printk(K_INFO, "%s\n", __func__);

	
	for (i = 1; i <= MAX_QMU_EP; i++) {

		mu3d_hal_stop_qmu(i, USB_TX);

		
		Tx_gpd_end[i] = Tx_gpd_last[i] = Tx_gpd_head[i];
		gpd_ptr_align(USB_TX, i, Tx_gpd_end[i]);
		free_gpd(USB_TX, i);

		
		os_writel(USB_QMU_TQSAR(i),
				mu3d_hal_gpd_virt_to_phys(Tx_gpd_last[i], USB_TX, i));
		qmu_printk(K_INFO, "USB_QMU_TQSAR %x\n", os_readl(USB_QMU_TQSAR(i)));
	}

	
	for (i = 1; i <= MAX_QMU_EP; i++) {
		mu3d_hal_stop_qmu(i, USB_RX);

		
		Rx_gpd_end[i] = Rx_gpd_last[i] = Rx_gpd_head[i];
		gpd_ptr_align(USB_RX, i, Rx_gpd_end[i]);
		free_gpd(USB_RX, i);

		
		os_writel(USB_QMU_RQSAR(i),
				mu3d_hal_gpd_virt_to_phys(Rx_gpd_end[i], USB_RX, i));
		qmu_printk(K_INFO, "USB_QMU_RQSAR %x\n", os_readl(USB_QMU_RQSAR(i)));
	}
}

#endif
