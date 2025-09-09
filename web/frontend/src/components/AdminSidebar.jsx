"use client";
import { useState } from "react";
import { Menu, X } from "lucide-react";
import { NavLink, useNavigate } from "react-router-dom";
import Swal from "sweetalert2";

export default function Sidebar() {
  const [isOpen, setIsOpen] = useState(false);
  const navigate = useNavigate();

  const menuItems = [
    { name: "Dashboard", path: "/admin", icon: "fa fa-area-chart" },
  ];

  const handleLogout = () => {
    setIsOpen(false)
    Swal.fire({
      title: "Are you sure?",
      text: "You will be logged out of your account.",
      icon: "warning",
      showCancelButton: true,
      confirmButtonColor: "#EB2525",
      cancelButtonColor: "#6c757d",
      confirmButtonText: "Yes, Log Out",
    }).then((result) => {
      if (result.isConfirmed) {
        // Hapus token / data login
        localStorage.removeItem("token");
        localStorage.removeItem("user");

        Swal.fire({
          title: "Logged Out",
          text: "You have successfully logged out.",
          icon: "success",
          timer: 1500,
          showConfirmButton: false,
        });

        // Redirect ke halaman login
        navigate("/");
      }
    });
  };

  return (
    <>
      {/* Toggle Button - Mobile */}
      <button
        className="md:hidden p-2 text-gray-600 fixed top-8 left-4 z-[10000]"
        onClick={() => setIsOpen(!isOpen)}
      >
        {isOpen ? <X size={24} /> : <Menu size={24} />}
      </button>

      {/* Sidebar */}
      <aside
        className={`fixed top-0 left-0 h-full bg-white shadow-lg p-4 transform 
      ${isOpen ? "translate-x-0" : "-translate-x-full"} 
      md:translate-x-0 transition-transform duration-200 w-72 z-[1060] px-4 pt-10 flex flex-col justify-between`}
      >
        <div>
          <div className="h-12 m-2 mb-10">
            <img src="img/admin-logo.svg" alt="Admin Logo" className="w-full" />
          </div>
          <nav className="space-y-3">
            {menuItems.map((item) => (
              <NavLink
                key={item.path}
                to={item.path}
                className={({ isActive }) =>
                  `font-semibold flex items-center gap-3 p-3 rounded-xl transition ${
                    isActive
                      ? "bg-[#EB2525] bg-opacity-10 text-[#EB2525]"
                      : "text-gray-700 hover:bg-gray-100"
                  }`
                }
              >
                <i className={item.icon}></i>
                {item.name}
              </NavLink>
            ))}
          </nav>
        </div>
        <div className="flex flex-col gap-4">
          <hr className="h-1 w-full bg-gray-200" />
          <button
            onClick={handleLogout}
            className="ml-2 flex items-center justify-between px-6 text-gray-700 hover:text-red-600"
          >
            Log Out
            <i className="fa fa-sign-out"></i>
          </button>
        </div>
      </aside>

      {/* Overlay for Mobile */}
      {isOpen && (
        <div
          className="fixed inset-0 bg-black bg-opacity-30 z-[1059] md:hidden"
          onClick={() => setIsOpen(false)}
        />
      )}
    </>
  );
}
