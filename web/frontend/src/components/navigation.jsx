import React, { useState, useEffect } from "react";
import { Link } from "react-router-dom";

export const Navigation = () => {
  const [isMobileMenuOpen, setIsMobileMenuOpen] = useState(false);
  const [isDesktop, setIsDesktop] = useState(window.innerWidth >= 1124);

  useEffect(() => {
    const handleResize = () => setIsDesktop(window.innerWidth >= 1124);
    window.addEventListener("resize", handleResize);
    return () => window.removeEventListener("resize", handleResize);
  }, []);

  const menuItems = [
    { name: "Features", href: "#features" },
    { name: "Live Camera", href: "#about" },
    { name: "Gallery", href: "#services" },
    { name: "Team", href: "#team" },
    { name: "Contact", href: "#contact" },
  ];

  return (
    <nav id="menu" className="navbar-default navbar-fixed-top bg-white shadow-md fixed !py-3">
      <div className="container mx-auto px-4">
        <div className="flex justify-between items-center align-center relative">
          {/* Brand */}
          <div className="navbar-header">
            <a
              className="page-scroll text-4xl font-bold !text-black navbar-brand flex items-center mt-3.5"
              href="#page-top"
            >
              SMART-<span className="text-[#EB2525]">TRAIN</span>
            </a>
          </div>

          {/* Desktop Menu */}
          {isDesktop ? (
            <ul className="navbar-nav flex space-x-6 items-center">
              {menuItems.map((item) => (
                <li key={item.name} className="relative">
                  <a
                    href={item.href}
                    className="hover:text-[#EB2525] transition duration-200 relative"
                  >
                    {item.name}
                  </a>
                </li>
              ))}
              <li>
                <Link
                  to="/login"
                  className="bg-[#EB2525] text-white px-4 py-2 !mt-6 rounded uppercase hover:bg-[#991B1B] transition duration-200 ml-4"
                >
                  Admin Page
                </Link>
              </li>
            </ul>
          ) : (
            <>
              {/* Hamburger Button */}
              <button
                className="p-2 focus:outline-none"
                onClick={() => setIsMobileMenuOpen(!isMobileMenuOpen)}
                aria-label="Toggle menu"
              >
                <div className="w-6 flex flex-col space-y-1">
                  <span
                    className={`h-0.5 bg-gray-800 transition-all duration-300 ${
                      isMobileMenuOpen ? "rotate-45 translate-y-1.5" : ""
                    }`}
                  ></span>
                  <span
                    className={`h-0.5 bg-gray-800 transition-all duration-300 ${
                      isMobileMenuOpen ? "opacity-0" : "opacity-100"
                    }`}
                  ></span>
                  <span
                    className={`h-0.5 bg-gray-800 transition-all duration-300 ${
                      isMobileMenuOpen ? "-rotate-45 -translate-y-1.5" : ""
                    }`}
                  ></span>
                </div>
              </button>

              {/* Mobile Menu */}
              {isMobileMenuOpen && (
                <div className="absolute top-full left-0 w-full bg-white shadow-lg z-50">
                  <ul className="py-2 px-4 space-y-3">
                    {menuItems.map((item) => (
                      <li key={item.name} className="relative">
                        <a
                          href={item.href}
                          onClick={() => setIsMobileMenuOpen(false)}
                          className="block py-2 hover:text-[#EB2525] transition duration-200 relative border-b border-gray-100"
                        >
                          {item.name}
                        </a>
                      </li>
                    ))}
                    <li>
                      <Link
                        to="/login"
                        className="block bg-[#EB2525] text-white px-4 py-2 rounded uppercase hover:bg-[#991B1B] transition duration-200 text-center"
                        onClick={() => setIsMobileMenuOpen(false)}
                      >
                        Admin Page
                      </Link>
                    </li>
                  </ul>
                </div>
              )}
            </>
          )}
        </div>
      </div>
    </nav>
  );
};
