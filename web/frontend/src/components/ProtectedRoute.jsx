// src/components/ProtectedRoute.jsx
import React from "react";
import { Navigate } from "react-router-dom";

const ProtectedRoute = ({ children }) => {
  const token = localStorage.getItem("token"); // cek token di localStorage

  if (!token) {
    // kalau ga ada token, redirect ke login
    return <Navigate to="/login" replace />;
  }

  return children; // kalau ada token, render children
};

export default ProtectedRoute;
