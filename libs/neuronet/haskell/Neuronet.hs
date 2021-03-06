{-# LANGUAGE ForeignFunctionInterface #-}

module Neuronet
where

import Foreign.C
import Foreign.C.String
import Foreign.Ptr

import Control.Monad
import Control.Applicative

type Packet a = Ptr a

packet_Push64 :: Packet a -> Double -> IO Int
packet_Push64 pkt num = 
	liftM fromIntegral (pkt_Push64 pkt ((fromRational . toRational) num))

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Push64" pkt_Push64 :: Ptr a -> CDouble -> IO CInt

packet_Push32 :: Packet a -> Int -> IO Int
packet_Push32 pkt num = 
	liftM fromIntegral (pkt_Push32 pkt (fromIntegral num))

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Push32" pkt_Push32 :: Ptr a -> CInt -> IO CInt

packet_Push16 :: Packet a -> Int -> IO Int
packet_Push16 pkt num = 
	liftM fromIntegral (pkt_Push16 pkt (fromIntegral num))

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Push16" pkt_Push16 :: Ptr a -> CShort -> IO CInt

packet_Push8 :: Packet a -> Int -> IO Int
packet_Push8 pkt num = 
	liftM fromIntegral (pkt_Push8 pkt (fromIntegral num))

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Push8" pkt_Push8 :: Ptr a -> CChar -> IO CInt

packet_PushString :: Packet a -> Int -> String -> IO Int
packet_PushString pkt len string =
	liftM fromIntegral (withCString string (\str -> pkt_PushString pkt (fromIntegral (len + 1)) str))

foreign import ccall unsafe "neuro/nnet/packet.h Packet_PushString" pkt_PushString :: Ptr a -> CUInt -> CString -> IO CInt

packet_Create :: IO (Packet a)
packet_Create =
	pkt_Create

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Create" pkt_Create :: IO (Ptr a)

packet_Destroy :: Packet a -> IO ()
packet_Destroy = pkt_Destroy

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Destroy" pkt_Destroy :: Ptr a -> IO ()

packet_GetLen :: Packet a -> IO Int
packet_GetLen pkt =
	liftM fromIntegral (pkt_GetLen pkt)

foreign import ccall unsafe "neuro/nnet/packet.h Packet_GetLen" pkt_GetLen :: Ptr a -> IO CInt

packet_GetBuffer :: Packet a -> IO String
packet_GetBuffer pkt =
	(pkt_GetBuffer pkt) >>= peekCString

foreign import ccall unsafe "neuro/nnet/packet.h Packet_GetBuffer" pkt_GetBuffer :: Ptr a -> IO CString

packet_Pop64 :: Packet a -> IO Double
packet_Pop64 pkt =
	liftM (fromRational . toRational) (pkt_Pop64 pkt)

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Pop64" pkt_Pop64 :: Ptr a -> IO CDouble

packet_Pop32 :: Packet a -> IO Int
packet_Pop32 pkt =
	liftM (fromIntegral) (pkt_Pop32 pkt)

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Pop32" pkt_Pop32 :: Ptr a -> IO CInt

packet_Pop16 :: Packet a -> IO Int
packet_Pop16 pkt =
	liftM (fromIntegral) (pkt_Pop16 pkt)

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Pop16" pkt_Pop16 :: Ptr a -> IO CShort

packet_Pop8 :: Packet a -> IO Int
packet_Pop8 pkt =
	liftM (fromIntegral) (pkt_Pop8 pkt)

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Pop8" pkt_Pop8 :: Ptr a -> IO CChar

packet_PopString :: Packet a -> IO String
packet_PopString pkt =
	pkt_PopString pkt >>= peekCString

foreign import ccall unsafe "neuro/nnet/packet.h Packet_PopString" pkt_PopString :: Ptr a -> IO CString

packet_PopData :: Packet a -> Int -> IO String
packet_PopData pkt len =
	pkt_PopData pkt (fromIntegral len) >>= \str -> peekCStringLen (str, len)

foreign import ccall unsafe "neuro/nnet/packet.h Packet_PopData" pkt_PopData :: Ptr a -> CUInt -> IO CString

packet_Reset :: Packet a -> IO ()
packet_Reset = pkt_Reset

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Reset" pkt_Reset :: Ptr a -> IO ()

packet_Set :: String -> Int -> IO (Packet a)
packet_Set string len =
	withCString string (\str -> pkt_Set str (fromIntegral len))

foreign import ccall unsafe "neuro/nnet/packet.h Packet_Set" pkt_Set :: CString -> CUInt -> IO (Ptr a)
